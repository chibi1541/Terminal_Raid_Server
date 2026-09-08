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

	길찾기가 보는 셀 격자. JPS는 오직 이것만 읽는다.
	셀 단위의 진실은 Level이 들고 있고, 여기는 그것을 액터 충돌 박스로 팽창시켜 구운 결과다.

	셀 (x,y) 는 "액터의 충돌 박스(boxCellsWide x boxCellsHigh, 위치가 중심) 를 그 셀에 놓았을 때
	벽에 안 걸리면" 통행 가능이다 = MoveMath::BoxBlockedCells 와 완전히 같은 판정 (이동 충돌과 동일).
	레벨 범위 밖 셀은 장애물이므로 박스가 맵 경계에 걸리는 셀도 자동으로 막힌다.
	(과거엔 footprint 타일로 셀을 묶었으나 이동 충돌 박스와 안 맞아 셀 공간으로 전환)

	TilePos 는 이제 "셀 좌표" 다 (1 노드 = 1 셀). 이름은 JPS/Room 호환을 위해 유지.
-------------*/

class NavGrid
{
public:
	// boxCellsWide/High : 이 격자를 쓸 액터의 충돌 박스 한 변(셀). 1x1 = 원시 셀 통행맵 그대로.
	void Build(const Vector<uint8>& cells, int32 cellWidth, int32 cellHeight,
			   int32 boxCellsWide, int32 boxCellsHigh);

	int32	GetWidth() const	{ return _width; }		// 셀 개수
	int32	GetHeight() const	{ return _height; }

	// 범위 밖은 항상 false.
	// 경계 검사를 호출부에 흩뿌리지 않으려고 여기서 삼킨다. JPS가 사방을 마음 놓고 물어볼 수 있다.
	// 호출 횟수를 센다 (_queryCount) - 길찾기가 실제로 검사한 셀 수의 프록시.
	bool	IsWalkable(int32 tx, int32 ty) const;

	// 통행 판정 질의 카운터. FindPath 진입에서 Reset, 종료에서 Get - "탐색한 노드 수".
	void	ResetQueryCount() const	{ _queryCount = 0; }
	uint64	GetQueryCount() const	{ return _queryCount; }

	int32	ToIndex(int32 tx, int32 ty) const { return ty * _width + tx; }
	TilePos	FromIndex(int32 index) const;

	// 셀 격자라 항등. (호출부 호환용으로 유지)
	TilePos				CellToTile(int32 cellX, int32 cellY) const { return { cellX, cellY }; }
	Protocol::Vector2	TileToCellCenter(TilePos tile) const;

	// 막힌 셀이 주어졌을 때 가장 가까운 통행 가능 셀을 링 탐색으로 찾는다.
	// from이 이미 통행 가능하면 그대로 돌려준다.
	bool	FindNearestWalkable(TilePos from, int32 maxRadius, OUT TilePos& outTile) const;

private:
	int32			_width = 0;
	int32			_height = 0;
	Vector<uint8>	_walkable;	// 1 = 통행 가능
	mutable uint64	_queryCount = 0;	// IsWalkable 호출 횟수 (탐색 노드 수 계측용)
};
