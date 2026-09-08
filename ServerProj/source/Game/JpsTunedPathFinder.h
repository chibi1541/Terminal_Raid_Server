#pragma once
#include "Game/IPathFinder.h"

/*------------------
	JpsTunedPathFinder

	원본은 JpsPathFinder. 이건 개활지 스캔 폭발을 줄이는 개선안 2종을 플래그로 켜는 비교용 복제다
	(원본 클래스를 안 건드리는 게 요구사항이라 상속 대신 자립 복사).

	  bias  : (A) 확장 시 휴리스틱 최소 방향(들)만 Jump 스캔, 나머지 방향은 1칸 노드로 오픈리스트에 미룬다.
	          개활지에서 골 반대쪽으로 맵 끝까지 걸어가는 낭비 점프를 없앤다.
	  bound : (B) Jump 스캔을 JUMP_MAX_* 로 제한, 초과하면 그 자리 셀을 중간 노드로 반환.
	          대각 Jump 의 재귀 직선 서브점프 O(L·W) 폭발을 O(L·N) 으로.

	둘 다 admissible 옥타일 휴리스틱 + 일관 g 를 유지하므로 최단 경로는 그대로다.
	(교과서 JPS 의 "노드 수 최소" 보장만 잃는다.)

	Configure(false,false) 면 원본 JpsPathFinder 와 동일하게 동작한다.

	주의 - 스레드 비안전 (스크래치 버퍼 재사용). 룸 잡 큐 안에서만.
-------------------*/

class JpsTunedPathFinder : public IPathFinder
{
	enum
	{
		COST_STRAIGHT		= 10,
		COST_DIAGONAL		= 14,
		DEFAULT_MAX_NODE	= 60000,		// defer/중간 노드로 확장 수가 늘어 원본(8192)보다 여유 필요
		JUMP_MAX_PRIMARY	= 96,			// 확장 루프가 부른 점프 : 이만큼 걷고 못 찾으면 중간 노드
		JUMP_MAX_RECURSE	= 48,			// 대각의 재귀 직선 서브점프 : 이만큼 걷고 못 찾으면 -1
	};

public:
	// bias : (A), bound : (B). bound 는 bias 와 독립이지만 보통 같이 켠다.
	void	Configure(bool bias, bool bound) { _bias = bias; _bound = bound; }

	bool	FindPath(const NavGrid& grid, TilePos start, TilePos goal,
					 OUT Vector<TilePos>& outPath, int32 maxNodeCount = 0) override;

	int32	GetLastExpandedCount() const override { return _lastExpanded; }
	int64	GetLastScannedCount() const override { return _lastScanned; }
	const Vector<TilePos>& GetLastOpenedNodes() const override { return _lastOpened; }
	const Vector<TilePos>& GetLastPathNodes() const override { return _jumpPoints; }

	void	SetRecordSearchNodes(bool on) override { _recordSearch = on; }
	const wchar_t* Name() const override { return _bound ? L"JPS_B" : (_bias ? L"JPS_A" : L"JPS*"); }

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

private:
	bool	FindPathImpl(const NavGrid& grid, TilePos start, TilePos goal,
						 OUT Vector<TilePos>& outPath, int32 maxNodeCount);

	// budget : 이만큼 걷고도 점프 포인트를 못 찾으면 중단. emitOnBudget 이면 그 자리 셀을 반환(중간 노드),
	//          아니면 -1(이 방향엔 점프 포인트 없음). bound 가 꺼져 있으면 budget 은 INT32_MAX 라 도달 안 함.
	int32	Jump(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy, TilePos goal,
				 int32 budget, bool emitOnBudget);
	bool	HasForcedNeighbour(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy);
	void	GetPrunedDirections(const NavGrid& grid, int32 index, OUT Vector<TilePos>& outDirs);
	void	BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath);

	// 확장 루프에서 successor 하나(점프 포인트 or 미룬 1칸 노드)를 오픈리스트에 반영.
	void	RelaxSuccessor(int32 currentIndex, TilePos currentPos, int32 currentG,
						   int32 succIndex, TilePos succPos, TilePos goal);

	static bool		CanStepDiagonal(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy);
	static int32	Heuristic(TilePos a, TilePos b);
	static int32	StepCost(TilePos from, TilePos to);

private:
	bool	_bias  = true;
	bool	_bound = false;

	Vector<NodeData>		_nodes;
	PriorityQueue<OpenNode>	_open;
	Vector<TilePos>			_dirs;
	Vector<TilePos>			_jumpPoints;
	Vector<TilePos>			_lastOpened;
	uint32					_stamp = 0;
	int32					_lastExpanded = 0;
	int64					_lastScanned = 0;
	bool					_recordSearch = true;
};
