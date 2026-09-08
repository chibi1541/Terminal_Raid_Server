#pragma once
#include "Game/IPathFinder.h"

/*------------------
	JpsPathFinder

	Jump Point Search. 코너 커팅을 금지하는 변형이다.
	대각으로 가려면 인접한 두 직교 타일이 모두 비어 있어야 한다
	=> 몬스터가 벽 모서리를 뚫고 지나가지 않는다.

	주의 - 스레드 안전하지 않다.
	탐색 스크래치 버퍼를 인스턴스가 들고 재사용하기 때문이다.
	Room이 JobQueue라 직렬 실행이 보장되므로 룸당 하나씩 두고 룸 잡 큐 안에서만 쓴다.
-------------------*/

class JpsPathFinder : public IPathFinder
{
	enum
	{
		COST_STRAIGHT		= 10,
		COST_DIAGONAL		= 14,	// √2의 정수 근사. float를 피해 힙 비교를 정확하게 유지한다
		DEFAULT_MAX_NODE	= 8192,	// 셀 공간 격자(380x280) - 긴/미로형 경로 확장 여유
	};

public:
	// 성공하면 outPath에 start부터 goal까지의 점프 포인트(방향 전환점)가 채워진다. (양 끝 포함)
	// 한 칸씩 펼치지 않는다 - 사이는 직선/대각이라 추종기가 한 방향으로 쭉 간다.
	bool	FindPath(const NavGrid& grid, TilePos start, TilePos goal,
					 OUT Vector<TilePos>& outPath, int32 maxNodeCount = 0) override;

	/* Debug : 마지막 탐색에서 확장한(=open에서 꺼낸) 점프 포인트 수 */
	int32	GetLastExpandedCount() const override { return _lastExpanded; }

	/* Debug : 마지막 탐색에서 검사한 셀 수 (Jump 스캔 포함). A* 와 비교 가능한 "일한 양". */
	int64	GetLastScannedCount() const override { return _lastScanned; }

	// Debug : 마지막 탐색에서 _open에 넣은 점프 포인트들.
	// 같은 타일이 더 나은 g로 다시 들어가면 중복으로 담긴다. 그리기용이라 걸러내지 않는다.
	const Vector<TilePos>& GetLastOpenedNodes() const override { return _lastOpened; }

	// Debug : 마지막 경로가 실제로 지나는 점프 포인트들. start -> goal 순서다.
	// 첫 원소는 항상 출발 타일이고 마지막 원소는 도착 타일이다.
	// 탐색에 실패하면 비어 있다.
	const Vector<TilePos>& GetLastPathNodes() const override { return _jumpPoints; }

	void	SetRecordSearchNodes(bool on) override { _recordSearch = on; }
	const wchar_t* Name() const override { return L"JPS"; }

private:
	struct NodeData
	{
		int32	g = 0;
		int32	parentIndex = -1;
		uint32	stamp = 0;			// 탐색 세대. 매 탐색마다 배열을 지우지 않기 위한 것
		bool	closed = false;
	};

	struct OpenNode
	{
		int32 index = 0;
		int32 f = 0;

		// PriorityQueue는 std::priority_queue라 기본이 max-heap이다.
		// JobTimer의 TimerItem과 같은 방식으로 부등호를 뒤집어 min-heap으로 쓴다.
		bool operator<(const OpenNode& other) const { return f > other.f; }
	};

private:
	// 실제 탐색. FindPath 가 NavGrid 질의 카운터를 리셋/수집하며 감싼다.
	bool	FindPathImpl(const NavGrid& grid, TilePos start, TilePos goal,
						 OUT Vector<TilePos>& outPath, int32 maxNodeCount);

	int32	Jump(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy, TilePos goal);
	bool	HasForcedNeighbour(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy);
	void	GetPrunedDirections(const NavGrid& grid, int32 index, OUT Vector<TilePos>& outDirs);
	void	BuildPath(const NavGrid& grid, int32 goalIndex, OUT Vector<TilePos>& outPath);

	static bool		CanStepDiagonal(const NavGrid& grid, int32 x, int32 y, int32 dx, int32 dy);
	static int32	Heuristic(TilePos a, TilePos b);
	static int32	StepCost(TilePos from, TilePos to);

private:
	Vector<NodeData>		_nodes;
	PriorityQueue<OpenNode>	_open;
	Vector<TilePos>			_dirs;			// GetPrunedDirections 결과 재사용
	Vector<TilePos>			_jumpPoints;	// 경로가 지나는 점프 포인트. BuildPath가 채우고 디버그가 읽는다
	Vector<TilePos>			_lastOpened;	// Debug : _open에 넣은 점프 포인트 기록
	uint32					_stamp = 0;
	int32					_lastExpanded = 0;
	int64					_lastScanned = 0;		// 마지막 탐색의 IsWalkable 호출 수
	bool					_recordSearch = true;	// _lastOpened 기록 여부 (구독자 없으면 Room이 끈다)
};
