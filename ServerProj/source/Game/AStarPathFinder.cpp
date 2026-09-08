#include "pch.h"
#include "Game/AStarPathFinder.h"
#include "Game/PathFinderCommon.h"

#include <algorithm>

namespace
{
	// 8방향. 직선 먼저, 대각 나중 (결과에 영향은 없고 탐색 순서만).
	constexpr int32 DIRS[8][2] =
	{
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 },
	};
}

void AStarPathFinder::BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath)
{
	_full.clear();

	for (int32 index = goalIndex; index >= 0; index = _nodes[index].parentIndex)
		_full.push_back(grid.FromIndex(index));

	std::reverse(_full.begin(), _full.end());

	// 셀별 경로 -> 방향 전환점만. JPS 점프포인트와 같은 sparse 형태.
	CompressCollinear(_full, OUT _pathNodes);

	for (const TilePos& n : _pathNodes)
		outPath.push_back(n);
}

bool AStarPathFinder::FindPath(const NavGrid& grid, TilePos start, TilePos goal,
							   OUT Vector<TilePos>& outPath, int32 maxNodeCount)
{
	grid.ResetQueryCount();
	const bool ok = FindPathImpl(grid, start, goal, OUT outPath, maxNodeCount);
	_lastScanned = static_cast<int64>(grid.GetQueryCount());
	return ok;
}

bool AStarPathFinder::FindPathImpl(const NavGrid& grid, TilePos start, TilePos goal,
								   OUT Vector<TilePos>& outPath, int32 maxNodeCount)
{
	if (maxNodeCount <= 0)
		maxNodeCount = DEFAULT_MAX_NODE;

	outPath.clear();
	_lastExpanded = 0;
	_lastOpened.clear();
	_pathNodes.clear();

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

	// 세대를 올려 지난 탐색 결과를 무효화한다 (배열을 매번 안 지우려고).
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

	_open.push(OpenNode{ startIndex, OctileHeuristic(start, goal) });

	while (_open.empty() == false)
	{
		const OpenNode current = _open.top();
		_open.pop();

		// 같은 노드가 더 나은 g 로 다시 들어갔던 경우, 낡은 항목은 여기서 버린다.
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
			LOG_WARN(L"[astar] search aborted : node limit %d reached", maxNodeCount);
			return false;
		}

		const TilePos pos = grid.FromIndex(current.index);
		const int32 curG = _nodes[current.index].g;

		for (const auto& d : DIRS)
		{
			const int32 dx = d[0];
			const int32 dy = d[1];
			const int32 nx = pos.x + dx;
			const int32 ny = pos.y + dy;

			if (grid.IsWalkable(nx, ny) == false)
				continue;

			// 코너 커팅 금지 - JPS / 이동 충돌과 동일한 규칙.
			if (dx != 0 && dy != 0 && CanStepDiagonalNoCut(grid, pos.x, pos.y, dx, dy) == false)
				continue;

			const int32 stepCost = (dx != 0 && dy != 0) ? PathCost::DIAGONAL : PathCost::STRAIGHT;
			const int32 newG = curG + stepCost;

			const int32 ni = grid.ToIndex(nx, ny);
			NodeData& nb = _nodes[ni];
			const bool visited = (nb.stamp == _stamp);

			if (visited && nb.closed)
				continue;

			if (visited && newG >= nb.g)
				continue;

			nb.g = newG;
			nb.parentIndex = current.index;
			nb.stamp = _stamp;
			nb.closed = false;

			_open.push(OpenNode{ ni, newG + OctileHeuristic(TilePos{ nx, ny }, goal) });

			if (_recordSearch)
				_lastOpened.push_back(TilePos{ nx, ny });
		}
	}

	return false;
}
