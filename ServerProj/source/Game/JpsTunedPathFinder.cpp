#include "pch.h"
#include "Game/JpsTunedPathFinder.h"
#include <algorithm>
#include <cstdint>

// 원본 JpsPathFinder.cpp 의 복제 + (A) 방향 편향 / (B) 점프 상한.
// 보조 함수(HasForcedNeighbour / CanStepDiagonal / Heuristic / StepCost / GetPrunedDirections / BuildPath)는
// 원본과 100% 동일하다 - 손대면 안 된다.

namespace
{
	int32 Sign(int32 value)
	{
		if (value > 0) return 1;
		if (value < 0) return -1;
		return 0;
	}
}

bool JpsTunedPathFinder::CanStepDiagonal(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy)
{
	return grid.IsWalkable(x + dx, y) && grid.IsWalkable(x, y + dy);
}

int32 JpsTunedPathFinder::Heuristic(TilePos a, TilePos b)
{
	const int32 dx = ::abs(a.x - b.x);
	const int32 dy = ::abs(a.y - b.y);
	return COST_STRAIGHT * (dx + dy) + (COST_DIAGONAL - 2 * COST_STRAIGHT) * ((dx < dy) ? dx : dy);
}

int32 JpsTunedPathFinder::StepCost(TilePos from, TilePos to)
{
	const int32 dx = ::abs(to.x - from.x);
	const int32 dy = ::abs(to.y - from.y);
	const int32 diagonal = (dx < dy) ? dx : dy;
	const int32 straight = ((dx > dy) ? dx : dy) - diagonal;
	return diagonal * COST_DIAGONAL + straight * COST_STRAIGHT;
}

bool JpsTunedPathFinder::HasForcedNeighbour(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy)
{
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

int32 JpsTunedPathFinder::Jump(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy, TilePos goal,
							   int32 budget, bool emitOnBudget)
{
	const bool diagonal = (dx != 0 && dy != 0);
	int32 scan = 0;

	while (true)
	{
		if (grid.IsWalkable(x, y) == false)
			return -1;

		if (x == goal.x && y == goal.y)
			return grid.ToIndex(x, y);

		// 대각 점프가 골의 행/열에 닿으면 여기서 멈춘다 (여기서부터는 직선으로 가야 최단).
		// 개활지에서 대각 점프가 맵 끝까지 걸어가는 것을 막는 핵심 조건. 장애물이 있어도 안전
		// (경로에 안 쓰이면 여분 점프 포인트일 뿐).
		if (dx != 0 && dy != 0 && (x == goal.x || y == goal.y))
			return grid.ToIndex(x, y);

		if (HasForcedNeighbour(grid, x, y, dx, dy))
			return grid.ToIndex(x, y);

		// (B) 예산 소진 : 확장 루프가 부른 점프면 이 자리 셀을 중간 노드로,
		//                 대각의 재귀 서브점프면 "여긴 점프 포인트 없음"(-1).
		if (++scan > budget)
			return emitOnBudget ? grid.ToIndex(x, y) : -1;

		if (diagonal)
		{
			if (Jump(grid, x + dx, y, dx, 0, goal, JUMP_MAX_RECURSE, /*emit*/ false) >= 0)
				return grid.ToIndex(x, y);

			if (Jump(grid, x, y + dy, 0, dy, goal, JUMP_MAX_RECURSE, /*emit*/ false) >= 0)
				return grid.ToIndex(x, y);

			if (CanStepDiagonal(grid, x, y, dx, dy) == false)
				return -1;
		}

		x += dx;
		y += dy;
	}
}

void JpsTunedPathFinder::GetPrunedDirections(const NavGrid& grid, int32 index, OUT Vector<TilePos>& outDirs)
{
	const NodeData& node = _nodes[index];
	const TilePos pos = grid.FromIndex(index);

	if (node.parentIndex < 0)
	{
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
		const bool vertical = grid.IsWalkable(pos.x, pos.y + dy);
		const bool horizontal = grid.IsWalkable(pos.x + dx, pos.y);

		if (vertical)
			outDirs.push_back(TilePos{ 0, dy });
		if (horizontal)
			outDirs.push_back(TilePos{ dx, 0 });
		if (vertical && horizontal)
			outDirs.push_back(TilePos{ dx, dy });
		return;
	}

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

void JpsTunedPathFinder::BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath)
{
	_jumpPoints.clear();

	for (int32 index = goalIndex; index >= 0; index = _nodes[index].parentIndex)
		_jumpPoints.push_back(grid.FromIndex(index));

	std::reverse(_jumpPoints.begin(), _jumpPoints.end());

	for (const TilePos& jp : _jumpPoints)
		outPath.push_back(jp);
}

void JpsTunedPathFinder::RelaxSuccessor(int32 currentIndex, TilePos currentPos, int32 currentG,
									   int32 succIndex, TilePos succPos, TilePos goal)
{
	NodeData& node = _nodes[succIndex];
	const bool visited = (node.stamp == _stamp);

	if (visited && node.closed)
		return;

	const int32 newG = currentG + StepCost(currentPos, succPos);

	if (visited && newG >= node.g)
		return;

	node.g = newG;
	node.parentIndex = currentIndex;
	node.stamp = _stamp;
	node.closed = false;

	_open.push(OpenNode{ succIndex, newG + Heuristic(succPos, goal) });

	if (_recordSearch)
		_lastOpened.push_back(succPos);
}

bool JpsTunedPathFinder::FindPath(const NavGrid& grid, TilePos start, TilePos goal,
								  OUT Vector<TilePos>& outPath, int32 maxNodeCount)
{
	grid.ResetQueryCount();
	const bool ok = FindPathImpl(grid, start, goal, OUT outPath, maxNodeCount);
	_lastScanned = static_cast<int64>(grid.GetQueryCount());
	return ok;
}

bool JpsTunedPathFinder::FindPathImpl(const NavGrid& grid, TilePos start, TilePos goal,
									  OUT Vector<TilePos>& outPath, int32 maxNodeCount)
{
	if (maxNodeCount <= 0)
		maxNodeCount = DEFAULT_MAX_NODE;

	outPath.clear();
	_lastExpanded = 0;
	_lastOpened.clear();
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

	const int32 primaryBudget = _bound ? static_cast<int32>(JUMP_MAX_PRIMARY) : INT32_MAX;

	// (A) 방향 편향에서 "지금까지 골에 가장 가까웠던" 휴리스틱. 이 값 이하인 노드(= 선두)만
	//     점프 스캔하고, 뒤처진(우회 중인) 노드는 1칸씩만 확장한다 (A* 처럼). 이러면 개활지
	//     직진은 점프로 빠르게, 장애물 우회는 A* 로 안전하게 - 미룬 노드가 값비싼 점프를 재발화
	//     하는 폭주를 막는다.
	int32 bestH = Heuristic(start, goal);

	while (_open.empty() == false)
	{
		const OpenNode current = _open.top();
		_open.pop();

		if (_nodes[current.index].closed)
			continue;

		_nodes[current.index].closed = true;
		_lastExpanded++;

		if (current.index == goalIndex)
		{
			BuildPath(grid, goalIndex, OUT outPath);
			return true;
		}

		if (_lastExpanded >= maxNodeCount)
		{
			LOG_WARN(L"[jps*] search aborted : node limit %d reached", maxNodeCount);
			return false;
		}

		const TilePos currentPos = grid.FromIndex(current.index);
		const int32 currentG = _nodes[current.index].g;

		_dirs.clear();
		GetPrunedDirections(grid, current.index, OUT _dirs);

		// (A) 방향 편향 : 이 노드가 선두(휴리스틱 <= bestH)면 골 쪽 최소-h 방향(들)만 Jump,
		//     나머지 방향은 1칸 노드로 미룬다. 뒤처진 노드는 전 방향 1칸만 (A* 처럼).
		const int32 curH = Heuristic(currentPos, goal);
		if (curH < bestH)
			bestH = curH;

		int32 minStepH = INT32_MAX;
		const bool atFrontier = (curH <= bestH);
		if (_bias)
		{
			for (const TilePos& d : _dirs)
			{
				const int32 h = Heuristic(TilePos{ currentPos.x + d.x, currentPos.y + d.y }, goal);
				if (h < minStepH)
					minStepH = h;
			}
		}

		for (const TilePos& dir : _dirs)
		{
			const TilePos step{ currentPos.x + dir.x, currentPos.y + dir.y };

			const bool jumpThisDir =
				(_bias == false) ||
				(atFrontier && Heuristic(step, goal) == minStepH);

			if (jumpThisDir == false)
			{
				// 미룬 방향 : 1칸 노드 그대로 push. _dirs 원소는 이미 walkable.
				RelaxSuccessor(current.index, currentPos, currentG,
							   grid.ToIndex(step.x, step.y), step, goal);
				continue;
			}

			const int32 jumpIndex = Jump(grid, step.x, step.y, dir.x, dir.y, goal,
										 primaryBudget, /*emitOnBudget*/ true);
			if (jumpIndex < 0)
				continue;

			RelaxSuccessor(current.index, currentPos, currentG,
						   jumpIndex, grid.FromIndex(jumpIndex), goal);
		}
	}

	return false;
}
