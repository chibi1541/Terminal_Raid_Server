#include "pch.h"
#include "Game/NavGrid.h"

void NavGrid::Build(const Vector<uint8>& cells, int32 cellWidth, int32 cellHeight, int32 tileSize)
{
	_tileSize = (tileSize > 0) ? tileSize : 1;

	// 나머지 셀이 남으면 그 타일도 하나로 친다. 어차피 잘린 타일은 아래에서 막힌 것으로 판정된다.
	_width = (cellWidth + _tileSize - 1) / _tileSize;
	_height = (cellHeight + _tileSize - 1) / _tileSize;

	_walkable.clear();
	_walkable.resize(static_cast<size_t>(_width) * _height, 0);

	for (int32 ty = 0; ty < _height; ty++)
	{
		for (int32 tx = 0; tx < _width; tx++)
		{
			bool walkable = true;

			for (int32 oy = 0; oy < _tileSize && walkable; oy++)
			{
				for (int32 ox = 0; ox < _tileSize; ox++)
				{
					const int32 cellX = tx * _tileSize + ox;
					const int32 cellY = ty * _tileSize + oy;

					// 레벨 범위 밖 = 장애물
					const bool blocked = (cellX >= cellWidth || cellY >= cellHeight)
						|| (cells[static_cast<size_t>(cellY) * cellWidth + cellX] != 0);

					if (blocked)
					{
						walkable = false;
						break;
					}
				}
			}

			_walkable[static_cast<size_t>(ty) * _width + tx] = walkable ? 1 : 0;
		}
	}
}

bool NavGrid::IsWalkable(int32 tx, int32 ty) const
{
	if (tx < 0 || ty < 0 || tx >= _width || ty >= _height)
		return false;

	return _walkable[static_cast<size_t>(ty) * _width + tx] != 0;
}

TilePos NavGrid::FromIndex(int32 index) const
{
	TilePos pos;

	if (_width > 0)
	{
		pos.x = index % _width;
		pos.y = index / _width;
	}

	return pos;
}

TilePos NavGrid::CellToTile(int32 cellX, int32 cellY) const
{
	TilePos pos;
	pos.x = cellX / _tileSize;
	pos.y = cellY / _tileSize;

	return pos;
}

Protocol::Vector2 NavGrid::TileToCellCenter(TilePos tile) const
{
	Protocol::Vector2 pos;
	pos.set_x(tile.x * _tileSize + _tileSize / 2);
	pos.set_y(tile.y * _tileSize + _tileSize / 2);

	return pos;
}

bool NavGrid::FindNearestWalkable(TilePos from, int32 maxRadius, OUT TilePos& outTile) const
{
	if (IsWalkable(from.x, from.y))
	{
		outTile = from;
		return true;
	}

	// 반지름을 넓혀가며 링 위를 훑는다. 같은 링 안에서는 실제 거리가 가장 가까운 것을 고른다.
	for (int32 radius = 1; radius <= maxRadius; radius++)
	{
		bool found = false;
		int32 bestDistSq = 0;
		TilePos best;

		for (int32 dy = -radius; dy <= radius; dy++)
		{
			for (int32 dx = -radius; dx <= radius; dx++)
			{
				// 링의 테두리만 본다. 안쪽은 이전 반복에서 이미 확인했다.
				if (::abs(dx) != radius && ::abs(dy) != radius)
					continue;

				const int32 tx = from.x + dx;
				const int32 ty = from.y + dy;

				if (IsWalkable(tx, ty) == false)
					continue;

				const int32 distSq = dx * dx + dy * dy;
				if (found == false || distSq < bestDistSq)
				{
					found = true;
					bestDistSq = distSq;
					best.x = tx;
					best.y = ty;
				}
			}
		}

		if (found)
		{
			outTile = best;
			return true;
		}
	}

	return false;
}
