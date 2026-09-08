#pragma once
#include "Game/NavGrid.h"

/*------------------
	IPathFinder

	길찾기 전략 인터페이스. JpsPathFinder / AStarPathFinder 가 구현한다.
	Room 이 활성 구현을 포인터로 들고, 콘솔 명령 pathalgo 로 교체한다.

	전부 스레드 비안전 (스크래치 버퍼 재사용). 룸 잡 큐 안에서만 쓸 것.
-------------------*/

enum class EPathFinder : uint8
{
	Jps   = 0,
	AStar = 1,
};

class IPathFinder
{
public:
	virtual ~IPathFinder() = default;

	// start..goal 셀 경로(방향 전환점만, 양끝 포함)를 outPath 에 채운다.
	// maxNodeCount <= 0 이면 구현체 기본 상한.
	virtual bool FindPath(const NavGrid& grid, TilePos start, TilePos goal,
						  OUT Vector<TilePos>& outPath, int32 maxNodeCount = 0) = 0;

	// 마지막 탐색에서 open 에서 꺼낸(= 확장한) 노드 수.
	// JPS 는 점프 포인트만 세므로 작다. A* 는 셀마다 세므로 크다.
	virtual int32 GetLastExpandedCount() const = 0;

	// 마지막 탐색에서 실제로 검사한 셀 수 (= NavGrid::IsWalkable 호출 수).
	// JPS 의 Jump 스캔까지 포함하므로 두 알고리즘을 사과 대 사과로 비교할 수 있다.
	virtual int64 GetLastScannedCount() const = 0;

	// 마지막 탐색에서 open 에 넣은 노드 좌표들. 디버그 오버레이 searchNodes 용 (중복 허용).
	virtual const Vector<TilePos>& GetLastOpenedNodes() const = 0;

	// 마지막 경로가 지나는 방향 전환점들 (start..goal). 실패 시 비어 있음.
	virtual const Vector<TilePos>& GetLastPathNodes() const = 0;

	// searchNodes 기록 on/off. A* 는 open 이 수만 개라 구독자 없을 땐 꺼서 비용을 아낀다.
	virtual void SetRecordSearchNodes(bool on) = 0;

	virtual const wchar_t* Name() const = 0;
};
