#pragma once
#include "Game/NavGrid.h"

/*------------------
	PathFinderCommon

	JPS 와 A* 가 공유하는 비용 모델 / 그리드 규칙 / 경로 후처리.
	두 길찾기의 규칙이 갈라지지 않도록 한 곳에 모은다.
-------------------*/

namespace PathCost
{
	constexpr int32 STRAIGHT = 10;
	constexpr int32 DIAGONAL = 14;	// √2 의 정수 근사. float 를 피해 힙 비교를 정확히 유지.
}

// 옥타일 거리. 8방향 이동에서 실제 비용을 절대 넘지 않는다(admissible).
int32 OctileHeuristic(TilePos a, TilePos b);

// 코너 커팅 금지 : 대각 (dx,dy) 로 가려면 인접한 두 직교 셀이 모두 열려 있어야 한다.
bool  CanStepDiagonalNoCut(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy);

// 한 칸 간격의 전체 셀 경로(양끝 포함)에서 연속 동일방향 구간을 접어 방향 전환점만 남긴다.
// A* 출력을 JPS 점프포인트와 같은 sparse 형태로 만들어 Room::UpdateMovement 추종기가 버벅이지 않게.
void  CompressCollinear(const Vector<TilePos>& fullCells, OUT Vector<TilePos>& outNodes);

// 인접 웨이포인트 사이 옥타일 비용의 합. 벤치에서 두 경로의 품질(길이) 비교용.
int32 PathTotalCost(const Vector<TilePos>& nodes);
