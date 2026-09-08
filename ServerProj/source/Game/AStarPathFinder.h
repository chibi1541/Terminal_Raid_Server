#pragma once
#include "Game/IPathFinder.h"

/*------------------
	AStarPathFinder

	격자 A*. JPS 와 같은 NavGrid 인터페이스 / 비용 모델 / 코너 커팅 금지 규칙을 쓴다.
	셀 단위로 전부 확장하므로 JPS 보다 노드를 훨씬 많이 본다 - 성능 비교용.

	경로는 부모 체인을 역추적한 뒤 연속 동일방향 구간을 병합해(CompressCollinear)
	JPS 점프포인트와 같은 sparse 형태로 낸다 (추종기 호환).

	주의 - 스레드 비안전 (스크래치 버퍼 재사용). 룸 잡 큐 안에서만 쓸 것.
-------------------*/

class AStarPathFinder : public IPathFinder
{
	enum
	{
		DEFAULT_MAX_NODE = 120000,	// 셀 격자(380x280 ~= 106k) 최악 대비 여유
	};

public:
	bool	FindPath(const NavGrid& grid, TilePos start, TilePos goal,
					 OUT Vector<TilePos>& outPath, int32 maxNodeCount = 0) override;

	int32	GetLastExpandedCount() const override { return _lastExpanded; }
	int64	GetLastScannedCount() const override { return _lastScanned; }
	const Vector<TilePos>& GetLastOpenedNodes() const override { return _lastOpened; }
	const Vector<TilePos>& GetLastPathNodes() const override { return _pathNodes; }

	void	SetRecordSearchNodes(bool on) override { _recordSearch = on; }
	const wchar_t* Name() const override { return L"A*"; }

private:
	struct NodeData
	{
		int32	g = 0;
		int32	parentIndex = -1;
		uint32	stamp = 0;
		bool	closed = false;
	};

	struct OpenNode
	{
		int32 index = 0;
		int32 f = 0;
		bool operator<(const OpenNode& other) const { return f > other.f; }	// min-heap
	};

	// 실제 탐색. FindPath 가 NavGrid 질의 카운터를 리셋/수집하며 감싼다.
	bool	FindPathImpl(const NavGrid& grid, TilePos start, TilePos goal,
						 OUT Vector<TilePos>& outPath, int32 maxNodeCount);

	void	BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath);

private:
	Vector<NodeData>		_nodes;
	PriorityQueue<OpenNode>	_open;
	Vector<TilePos>			_full;			// 역추적한 셀별 전체 경로 (병합 전)
	Vector<TilePos>			_pathNodes;		// 방향 전환점만 (GetLastPathNodes)
	Vector<TilePos>			_lastOpened;	// Debug : open 에 넣은 셀들
	uint32					_stamp = 0;
	int32					_lastExpanded = 0;
	int64					_lastScanned = 0;
	bool					_recordSearch = false;
};
