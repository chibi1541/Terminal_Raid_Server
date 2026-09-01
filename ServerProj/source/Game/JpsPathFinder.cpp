#include "pch.h"
#include "Game/JpsPathFinder.h"
#include <algorithm>

namespace
{
	int32 Sign(int32 value)
	{
		if (value > 0)
			return 1;

		if (value < 0)
			return -1;

		return 0;
	}
}

/*---------------
	보조 함수
----------------*/

bool JpsPathFinder::CanStepDiagonal(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy)
{
	// 코너 커팅 금지 : 대각으로 가려면 인접한 두 직교 타일이 모두 열려 있어야 한다.
	return grid.IsWalkable(x + dx, y) && grid.IsWalkable(x, y + dy);
}

int32 JpsPathFinder::Heuristic(TilePos a, TilePos b)
{
	// 옥타일 거리. 8방향 이동에서 실제 비용을 절대 넘지 않는다(admissible).
	const int32 dx = ::abs(a.x - b.x);
	const int32 dy = ::abs(a.y - b.y);

	return COST_STRAIGHT * (dx + dy) + (COST_DIAGONAL - 2 * COST_STRAIGHT) * ((dx < dy) ? dx : dy);
}

int32 JpsPathFinder::StepCost(TilePos from, TilePos to)
{
	// 점프 포인트는 항상 직선이나 대각선 위에 있으므로 이렇게 계산할 수 있다.
	const int32 dx = ::abs(to.x - from.x);
	const int32 dy = ::abs(to.y - from.y);

	const int32 diagonal = (dx < dy) ? dx : dy;
	const int32 straight = ((dx > dy) ? dx : dy) - diagonal;

	return diagonal * COST_DIAGONAL + straight * COST_STRAIGHT;
}

/*-------------------
	강제 이웃 판정
--------------------*/

bool JpsPathFinder::HasForcedNeighbour(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy)
{
	// 코너 커팅을 금지하면 강제 이웃은 직선 이동에서만, 그리고 수직 방향으로만 생긴다.
	//
	// 수평으로 (dx,0) 이동해 n=(x,y)에 왔다고 하자. 부모는 p=(x-dx,y)다.
	// (x,y+1)이 열려 있는데 (x-dx,y+1)이 막혀 있으면
	// p에서 (x,y+1)로 곧장 가는 대각 이동이 코너 커팅이라 불가능하다.
	// 즉 p -> n -> (x,y+1) 말고는 길이 없으므로 n이 점프 포인트가 된다.
	//
	// 대각 이동에는 강제 이웃 검사가 없다. 대각선 위의 점프 포인트는
	// Jump()가 돌리는 두 직선 탐색이 대신 찾아준다.

	if (dx != 0 && dy != 0)
		return false;

	if (dx != 0)
	{
		if (grid.IsWalkable(x, y - 1) && grid.IsWalkable(x - dx, y - 1) == false)
			return true;

		if (grid.IsWalkable(x, y + 1) && grid.IsWalkable(x - dx, y + 1) == false)
			return true;

		return false;
	}

	if (grid.IsWalkable(x - 1, y) && grid.IsWalkable(x - 1, y - dy) == false)
		return true;

	if (grid.IsWalkable(x + 1, y) && grid.IsWalkable(x + 1, y - dy) == false)
		return true;

	return false;
}

/*---------
	Jump
----------*/

int32 JpsPathFinder::Jump(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy, TilePos goal)
{
	// (x, y)는 이미 한 칸 내디딘 결과다.
	// 직선/대각 모두 루프로 전진한다. 재귀는 대각에서 직선 두 갈래를 볼 때만 쓴다.
	// (직선까지 재귀로 짜면 긴 복도에서 스택 깊이가 복도 길이만큼 쌓인다)
	const bool diagonal = (dx != 0 && dy != 0);

	while (true)
	{
		if (grid.IsWalkable(x, y) == false)
			return -1;

		if (x == goal.x && y == goal.y)
			return grid.ToIndex(x, y);

		if (HasForcedNeighbour(grid, x, y, dx, dy))
			return grid.ToIndex(x, y);

		if (diagonal)
		{
			// 대각선 위의 노드에서 직선 두 방향에 점프 포인트가 있으면 이 노드도 점프 포인트다.
			if (Jump(grid, x + dx, y, dx, 0, goal) >= 0)
				return grid.ToIndex(x, y);

			if (Jump(grid, x, y + dy, 0, dy, goal) >= 0)
				return grid.ToIndex(x, y);

			// 다음 대각 칸으로 넘어갈 수 있는지 (코너 커팅 금지)
			if (CanStepDiagonal(grid, x, y, dx, dy) == false)
				return -1;
		}

		x += dx;
		y += dy;
	}
}

/*------------------------
	가지치기된 이웃 방향
-------------------------*/

void JpsPathFinder::GetPrunedDirections(const NavGrid& grid, int32 index, OUT Vector<TilePos>& outDirs)
{
	const NodeData& node = _nodes[index];
	const TilePos pos = grid.FromIndex(index);

	if (node.parentIndex < 0)
	{
		// 시작 노드는 부모가 없으니 진입 방향도 없다. 8방향을 다 열어준다.
		for (int32 dy = -1; dy <= 1; dy++)
		{
			for (int32 dx = -1; dx <= 1; dx++)
			{
				if (dx == 0 && dy == 0)
					continue;

				if (dx != 0 && dy != 0 && CanStepDiagonal(grid, pos.x, pos.y, dx, dy) == false)
					continue;

				if (grid.IsWalkable(pos.x + dx, pos.y + dy) == false)
					continue;

				outDirs.push_back(TilePos{ dx, dy });
			}
		}

		return;
	}

	const TilePos parent = grid.FromIndex(node.parentIndex);
	const int32 dx = Sign(pos.x - parent.x);
	const int32 dy = Sign(pos.y - parent.y);

	if (dx != 0 && dy != 0)
	{
		// 대각으로 진입
		const bool vertical = grid.IsWalkable(pos.x, pos.y + dy);
		const bool horizontal = grid.IsWalkable(pos.x + dx, pos.y);

		if (vertical)
			outDirs.push_back(TilePos{ 0, dy });

		if (horizontal)
			outDirs.push_back(TilePos{ dx, 0 });

		// 코너 커팅 금지라 두 직교가 모두 열려 있을 때만 대각을 잇는다.
		if (vertical && horizontal)
			outDirs.push_back(TilePos{ dx, dy });

		return;
	}

	// 직선으로 진입.
	//
	// 코너 커팅을 금지하면 가지치기가 약해진다. 대각 이동에 제약이 붙는 탓에
	// 수직으로 한 칸 비켜가는 것이 유일한 우회로인 경우가 생기기 때문에,
	// 수직 방향은 강제 이웃일 때만이 아니라 열려 있으면 항상 열어줘야 한다.
	// 커팅 허용 버전보다 노드를 더 보지만 이쪽이 옳다.
	if (dx != 0)
	{
		if (grid.IsWalkable(pos.x + dx, pos.y))
		{
			outDirs.push_back(TilePos{ dx, 0 });

			if (grid.IsWalkable(pos.x, pos.y + 1))
				outDirs.push_back(TilePos{ dx, 1 });

			if (grid.IsWalkable(pos.x, pos.y - 1))
				outDirs.push_back(TilePos{ dx, -1 });
		}

		if (grid.IsWalkable(pos.x, pos.y + 1))
			outDirs.push_back(TilePos{ 0, 1 });

		if (grid.IsWalkable(pos.x, pos.y - 1))
			outDirs.push_back(TilePos{ 0, -1 });

		return;
	}

	if (grid.IsWalkable(pos.x, pos.y + dy))
	{
		outDirs.push_back(TilePos{ 0, dy });

		if (grid.IsWalkable(pos.x + 1, pos.y))
			outDirs.push_back(TilePos{ 1, dy });

		if (grid.IsWalkable(pos.x - 1, pos.y))
			outDirs.push_back(TilePos{ -1, dy });
	}

	if (grid.IsWalkable(pos.x + 1, pos.y))
		outDirs.push_back(TilePos{ 1, 0 });

	if (grid.IsWalkable(pos.x - 1, pos.y))
		outDirs.push_back(TilePos{ -1, 0 });
}

/*-------------
	경로 복원
--------------*/

void JpsPathFinder::BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath)
{
	_jumpPoints.clear();

	for (int32 index = goalIndex; index >= 0; index = _nodes[index].parentIndex)
		_jumpPoints.push_back(grid.FromIndex(index));

	// goal -> start 순으로 쌓였으니 뒤집는다.
	std::reverse(_jumpPoints.begin(), _jumpPoints.end());

	outPath.push_back(_jumpPoints[0]);

	// 점프 포인트 사이는 항상 직선이나 대각선이다. 한 칸씩 채워 타일 경로로 펼친다.
	for (size_t i = 1; i < _jumpPoints.size(); i++)
	{
		const TilePos from = _jumpPoints[i - 1];
		const TilePos to = _jumpPoints[i];

		const int32 stepX = Sign(to.x - from.x);
		const int32 stepY = Sign(to.y - from.y);

		TilePos cursor = from;
		while (cursor != to)
		{
			cursor.x += stepX;
			cursor.y += stepY;
			outPath.push_back(cursor);
		}
	}
}

/*-------------
	FindPath
--------------*/

bool JpsPathFinder::FindPath(const NavGrid& grid, TilePos start, TilePos goal,
							 OUT Vector<TilePos>& outPath, int32 maxNodeCount)
{
	outPath.clear();
	_lastExpanded = 0;
	_lastOpened.clear();
	// 실패하고 빠져나가는 경로에서도 지난 탐색 결과가 남아 있으면 안 된다.
	_jumpPoints.clear();

	const int32 width = grid.GetWidth();
	const int32 height = grid.GetHeight();

	if (width <= 0 || height <= 0)
		return false;

	if (grid.IsWalkable(start.x, start.y) == false || grid.IsWalkable(goal.x, goal.y) == false)
		return false;

	if (start == goal)
	{
		outPath.push_back(start);
		return true;
	}

	const int32 nodeCount = width * height;
	if (static_cast<int32>(_nodes.size()) != nodeCount)
	{
		_nodes.clear();
		_nodes.resize(nodeCount);
		_stamp = 0;
	}

	// 세대를 올려 지난 탐색 결과를 무효화한다. 배열을 매번 지우지 않기 위한 것.
	// 한 바퀴를 다 돌았을 때만 실제로 초기화한다.
	_stamp++;
	if (_stamp == 0)
	{
		for (NodeData& node : _nodes)
			node.stamp = 0;

		_stamp = 1;
	}

	while (_open.empty() == false)
		_open.pop();

	const int32 startIndex = grid.ToIndex(start.x, start.y);
	const int32 goalIndex = grid.ToIndex(goal.x, goal.y);

	NodeData& startNode = _nodes[startIndex];
	startNode.g = 0;
	startNode.parentIndex = -1;
	startNode.stamp = _stamp;
	startNode.closed = false;

	_open.push(OpenNode{ startIndex, Heuristic(start, goal) });

	while (_open.empty() == false)
	{
		const OpenNode current = _open.top();
		_open.pop();

		// 같은 노드가 더 나은 g로 다시 들어갔던 경우, 낡은 항목은 여기서 버린다.
		if (_nodes[current.index].closed)
			continue;

		_nodes[current.index].closed = true;
		_lastExpanded++;

		if (current.index == goalIndex)
		{
			BuildPath(grid, goalIndex, OUT outPath);
			return true;
		}

		// 길찾기 하나가 룸 틱을 통째로 잡아먹지 않게 끊는다.
		if (_lastExpanded >= maxNodeCount)
		{
			LOG_WARN(L"[jps] search aborted : node limit %d reached", maxNodeCount);
			return false;
		}

		const TilePos currentPos = grid.FromIndex(current.index);
		const int32 currentG = _nodes[current.index].g;

		_dirs.clear();
		GetPrunedDirections(grid, current.index, OUT _dirs);

		for (const TilePos& dir : _dirs)
		{
			const int32 jumpIndex = Jump(grid, currentPos.x + dir.x, currentPos.y + dir.y,
										 dir.x, dir.y, goal);
			if (jumpIndex < 0)
				continue;

			NodeData& jumpNode = _nodes[jumpIndex];
			const bool visited = (jumpNode.stamp == _stamp);

			if (visited && jumpNode.closed)
				continue;

			const TilePos jumpPos = grid.FromIndex(jumpIndex);
			const int32 newG = currentG + StepCost(currentPos, jumpPos);

			if (visited && newG >= jumpNode.g)
				continue;

			jumpNode.g = newG;
			jumpNode.parentIndex = current.index;
			jumpNode.stamp = _stamp;
			jumpNode.closed = false;

			_open.push(OpenNode{ jumpIndex, newG + Heuristic(jumpPos, goal) });
			_lastOpened.push_back(jumpPos);
		}
	}

	return false;
}
