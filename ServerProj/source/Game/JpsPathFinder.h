#pragma once
#include "Game/NavGrid.h"

/*------------------
	JpsPathFinder

	Jump Point Search. 코너 커팅을 금지하는 변형이다.
	대각으로 가려면 인접한 두 직교 타일이 모두 비어 있어야 한다
	=> 몬스터가 벽 모서리를 뚫고 지나가지 않는다.

	주의 - 스레드 안전하지 않다.
	탐색 스크래치 버퍼를 인스턴스가 들고 재사용하기 때문이다.
	Room이 JobQueue라 직렬 실행이 보장되므로 룸당 하나씩 두고 룸 잡 큐 안에서만 쓴다.
-------------------*/

class JpsPathFinder
{
	enum
	{
		COST_STRAIGHT		= 10,
		COST_DIAGONAL		= 14,	// √2의 정수 근사. float를 피해 힙 비교를 정확하게 유지한다
		DEFAULT_MAX_NODE	= 1024,
	};

public:
	// 성공하면 outPath에 start부터 goal까지의 타일 경로가 채워진다. (양 끝 포함)
	// 점프 포인트만이 아니라 그 사이를 한 칸씩 펼친 결과다.
	bool	FindPath(const NavGrid& grid, TilePos start, TilePos goal,
					 OUT Vector<TilePos>& outPath, int32 maxNodeCount = DEFAULT_MAX_NODE);

	/* Debug : 마지막 탐색에서 확장한(=open에서 꺼낸) 노드 수 */
	int32	GetLastExpandedCount() const { return _lastExpanded; }

	// Debug : 마지막 탐색에서 _open에 넣은 점프 포인트들.
	// 같은 타일이 더 나은 g로 다시 들어가면 중복으로 담긴다. 그리기용이라 걸러내지 않는다.
	const Vector<TilePos>& GetLastOpenedJumpPoints() const { return _lastOpened; }

	// Debug : 마지막 경로가 실제로 지나는 점프 포인트들. start -> goal 순서다.
	// 첫 원소는 항상 출발 타일이고 마지막 원소는 도착 타일이다.
	// 탐색에 실패하면 비어 있다.
	const Vector<TilePos>& GetLastPathJumpPoints() const { return _jumpPoints; }

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
};
