#include "pch.h"
#include "Game/PathFinderCommon.h"

namespace
{
	int32 Sign(int32 value)
	{
		if (value > 0) return 1;
		if (value < 0) return -1;
		return 0;
	}
}

int32 OctileHeuristic(TilePos a, TilePos b)
{
	const int32 dx = ::abs(a.x - b.x);
	const int32 dy = ::abs(a.y - b.y);
	return PathCost::STRAIGHT * (dx + dy)
		+ (PathCost::DIAGONAL - 2 * PathCost::STRAIGHT) * ((dx < dy) ? dx : dy);
}

bool CanStepDiagonalNoCut(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy)
{
	return grid.IsWalkable(x + dx, y) && grid.IsWalkable(x, y + dy);
}

void CompressCollinear(const Vector<TilePos>& fullCells, OUT Vector<TilePos>& outNodes)
{
	outNodes.clear();

	const int32 n = static_cast<int32>(fullCells.size());
	if (n == 0)
		return;

	outNodes.push_back(fullCells[0]);

	for (int32 i = 1; i + 1 < n; i++)
	{
		const int32 inX  = Sign(fullCells[i].x - fullCells[i - 1].x);
		const int32 inY  = Sign(fullCells[i].y - fullCells[i - 1].y);
		const int32 outX = Sign(fullCells[i + 1].x - fullCells[i].x);
		const int32 outY = Sign(fullCells[i + 1].y - fullCells[i].y);

		if (inX != outX || inY != outY)
			outNodes.push_back(fullCells[i]);
	}

	if (n > 1)
		outNodes.push_back(fullCells[n - 1]);
}

int32 PathTotalCost(const Vector<TilePos>& nodes)
{
	int32 total = 0;

	for (size_t i = 1; i < nodes.size(); i++)
	{
		const int32 dx = ::abs(nodes[i].x - nodes[i - 1].x);
		const int32 dy = ::abs(nodes[i].y - nodes[i - 1].y);
		const int32 diag = (dx < dy) ? dx : dy;
		const int32 straight = ((dx > dy) ? dx : dy) - diag;
		total += diag * PathCost::DIAGONAL + straight * PathCost::STRAIGHT;
	}

	return total;
}
