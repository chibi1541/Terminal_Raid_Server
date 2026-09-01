#pragma once
#include "Protocol/Struct.pb.h"

/*------------
	TilePos
-------------*/

struct TilePos
{
	int32 x = 0;
	int32 y = 0;

	bool operator==(const TilePos& other) const { return x == other.x && y == other.y; }
	bool operator!=(const TilePos& other) const { return !(*this == other); }
};

/*------------
	NavGrid

	길찾기가 보는 타일 격자. JPS는 오직 이것만 읽는다.
	셀 단위의 진실은 Level이 들고 있고, 여기는 그것을 구운 결과다.

	타일 통행 판정은 보수적이다 - 타일 안의 셀이 하나라도 막혀 있으면 그 타일은 막힌 것으로 본다.
	액터가 셀 단위로 움직이기 때문에, 부분적으로 막힌 타일을 통과 가능으로 두면
	경로는 나오는데 실제로 걷다가 벽에 걸린다.
	레벨 범위 밖의 셀도 장애물로 세므로 경계에서 잘린 타일은 자동으로 막힌다.
-------------*/

class NavGrid
{
public:
	void Build(const Vector<uint8>& cells, int32 cellWidth, int32 cellHeight, int32 tileSize);

	int32	GetWidth() const	{ return _width; }		// 타일 개수
	int32	GetHeight() const	{ return _height; }
	int32	GetTileSize() const	{ return _tileSize; }

	// 범위 밖은 항상 false.
	// 경계 검사를 호출부에 흩뿌리지 않으려고 여기서 삼킨다. JPS가 사방을 마음 놓고 물어볼 수 있다.
	bool	IsWalkable(int32 tx, int32 ty) const;

	int32	ToIndex(int32 tx, int32 ty) const { return ty * _width + tx; }
	TilePos	FromIndex(int32 index) const;

	TilePos				CellToTile(int32 cellX, int32 cellY) const;
	Protocol::Vector2	TileToCellCenter(TilePos tile) const;

	// 막힌 타일이 주어졌을 때 가장 가까운 통행 가능 타일을 링 탐색으로 찾는다.
	// from이 이미 통행 가능하면 그대로 돌려준다.
	bool	FindNearestWalkable(TilePos from, int32 maxRadius, OUT TilePos& outTile) const;

private:
	int32			_width = 0;
	int32			_height = 0;
	int32			_tileSize = 3;
	Vector<uint8>	_walkable;	// 1 = 통행 가능
};
