#include "pch.h"
#include "Game/NavGrid.h"

void NavGrid::Build(const Vector<uint8>& cells, int32 cellWidth, int32 cellHeight,
					 int32 boxCellsWide, int32 boxCellsHigh)
{
	const int32 boxW = (boxCellsWide > 0) ? boxCellsWide : 1;
	const int32 boxH = (boxCellsHigh > 0) ? boxCellsHigh : 1;

	// 격자 = 셀 그대로. 다운샘플 없음.
	_width = cellWidth;
	_height = cellHeight;

	_walkable.clear();
	_walkable.resize(static_cast<size_t>(_width) * _height, 0);

	if (_width <= 0 || _height <= 0)
		return;

	// 요약면적표(summed-area table) : prefix[(y)*(w+1) + (x)] = [0,x) x [0,y) 안의 막힌 셀 개수.
	// 이걸로 임의 박스의 "막힌 셀 몇 개" 를 O(1) 로 물어본다 → box 크기와 무관하게 O(cells) 빌드.
	const int32 pw = _width + 1;
	Vector<int32> prefix;
	prefix.resize(static_cast<size_t>(pw) * (_height + 1), 0);

	for (int32 y = 0; y < _height; y++)
	{
		for (int32 x = 0; x < _width; x++)
		{
			const int32 blocked = (cells[static_cast<size_t>(y) * _width + x] != 0) ? 1 : 0;
			prefix[static_cast<size_t>(y + 1) * pw + (x + 1)] =
				prefix[static_cast<size_t>(y) * pw + (x + 1)]
				+ prefix[static_cast<size_t>(y + 1) * pw + x]
				- prefix[static_cast<size_t>(y) * pw + x]
				+ blocked;
		}
	}

	// [x0,x1] x [y0,y1] (양끝 포함) 안의 막힌 셀 개수.
	auto rectBlocked = [&](int32 x0, int32 y0, int32 x1, int32 y1) -> int32
	{
		return prefix[static_cast<size_t>(y1 + 1) * pw + (x1 + 1)]
			- prefix[static_cast<size_t>(y0) * pw + (x1 + 1)]
			- prefix[static_cast<size_t>(y1 + 1) * pw + x0]
			+ prefix[static_cast<size_t>(y0) * pw + x0];
	};

	for (int32 y = 0; y < _height; y++)
	{
		for (int32 x = 0; x < _width; x++)
		{
			// MoveMath::BoxBlockedCells 와 정확히 같은 앵커 : 중심 - box/2, 폭 box (짝수는 우/하로 한 칸 편향).
			const int32 minX = x - boxW / 2;
			const int32 maxX = minX + boxW - 1;
			const int32 minY = y - boxH / 2;
			const int32 maxY = minY + boxH - 1;

			bool walkable;
			if (minX < 0 || minY < 0 || maxX >= _width || maxY >= _height)
				walkable = false;	// 박스가 맵 밖으로 걸침 = 벽 (레벨 경계 = 장애물)
			else
				walkable = (rectBlocked(minX, minY, maxX, maxY) == 0);

			_walkable[static_cast<size_t>(y) * _width + x] = walkable ? 1 : 0;
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

Protocol::Vector2 NavGrid::TileToCellCenter(TilePos tile) const
{
	// 셀 격자라 항등.
	Protocol::Vector2 pos;
	pos.set_x(tile.x);
	pos.set_y(tile.y);

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
