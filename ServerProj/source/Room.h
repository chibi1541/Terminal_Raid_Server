#pragma once
#include "JobQueue.h"
#include "Game/GameObject.h"
#include "Game/Level.h"
#include "Game/JpsPathFinder.h"
#include "Game/QuadTree.h"
#include "AI/BehaviorTreeManager.h"
#include "AI/BtInstance.h"

class Player;

/*---------
	Room

	하나의 게임 월드.

	JobQueue를 상속하므로 이 룸에 밀어넣은 잡은 항상 직렬로 실행된다.
	따라서 _objects를 비롯한 룸 상태에는 락을 걸지 않는다.
	대신 룸 상태를 만지는 코드는 반드시 DoAsync / DoTimer를 통해 들어와야 한다.
	(패킷 핸들러도, 디버그 명령도 예외 없이)
----------*/

class Room : public JobQueue
{
	enum
	{
		TICK_INTERVAL_MS		= 50,
		SPAWN_MAX_TRY			= 64,	// 통행 가능한 셀을 무작위로 찾을 때의 시도 횟수
		SNAP_MAX_RADIUS			= 8,	// 막힌 타일을 통행 가능한 타일로 스냅할 때의 최대 반경
		MOVE_KEYFRAME_INTERVAL	= 10,	// 이 틱마다 움직이는 액터를 강제로 브로드캐스트 (500ms 드리프트 보정)
		PROJECTILE_LIFETIME_TICKS	= 100,	// 투사체 기본 수명 (5초 @ 20Hz)
	};

public:
	Room();
	~Room();

	// useRandomSpawnPos가 false면 호출자가 미리 넣어둔 pos를 그대로 쓴다.
	// (디버그 spawn 명령이 좌표를 지정하는 경우)
	//
	// 주의 - JobQueue::DoAsync는 멤버 함수 시그니처와 인자 양쪽에서 Args를 추론하므로
	//        기본 인자가 통하지 않는다. 항상 두 인자를 다 넘길 것.
	void	Enter(GameObjectRef object, bool useRandomSpawnPos);
	void	Leave(GameObjectRef object);

	// exceptId에 해당하는 개체에게는 보내지 않는다.
	void	Broadcast(SendBufferRef sendBuffer, uint64 exceptId);

	void	BeginPlay();
	void	Tick();

	// 현재 서버 틱. Tick() 마다 1 증가. 이동 패킷의 보간 기준이다.
	uint64	GetTickCount() const { return _tickCount; }

	Protocol::Vector2	FindSpawnPos();
	GameObjectRef		Find(uint64 objectId);

	/*----------
		이동 (Movement)

		전부 룸 잡 큐 안에서만 부를 것.
	-----------*/

	// 플레이어 입력. 방향 홀드 - dir 이 DIR_NONE 이면 정지.
	// C_MOVE 핸들러가 DoAsync 로 넘긴다.
	void	HandleMove(GameObjectRef object, uint32 inputSeq, uint32 clientTick, int32 dir);

	// 목표 셀까지 JPS 경로를 깔고 추종 시작. 디버그 goto / 향후 AI 리프가 공유한다.
	bool	OrderMoveTo(uint64 objectId, int32 cellX, int32 cellY);

	// 직진 투사체를 스폰해 룸에 넣는다. cellsPerSec <= 0 이면 기본 속도,
	// lifetimeTicks <= 0 이면 기본 수명. 실패하면 nullptr.
	GameObjectRef	SpawnProjectile(int32 cellX, int32 cellY, Protocol::DirectionType dir,
									int32 cellsPerSec, int32 lifetimeTicks);

	// 디버그 : 이동 루프만 count 틱 수동으로 굴린다. (bt step 과 같은 방식)
	void	DebugStepMovement(int32 count);

	/*----------
		이벤트성 상태 (Hit / Death / Attack)

		위치처럼 매 틱 바뀌는 게 아니라 "이 순간 한 번" 일어나는 것들.
		S_MOVE 배치에 얹지 않고 별도 패킷으로 즉시 브로드캐스트한다.
	-----------*/

	// 데미지를 적용하고 S_HIT 브로드캐스트. 죽었으면 S_DEATH 도 이어서 브로드캐스트하고
	// 룸에서 내보낸다 (기존 S_DESPAWN 경로 재사용). 대상이 없거나 이미 죽었으면 false.
	bool	DealDamage(uint64 attackerId, uint64 targetId, int32 damage);

	// 데미지 판정과 무관하게 "공격 모션이 시작됐다"만 알린다.
	void	NotifyAttackStart(uint64 objectId, Protocol::DirectionType dir);

	uint32	GetWidth() const	{ return static_cast<uint32>(_level.GetWidth()); }
	uint32	GetHeight() const	{ return static_cast<uint32>(_level.GetHeight()); }

	const Level& GetLevel() const { return _level; }

	/*----------
		길찾기

		전부 룸 잡 큐 안에서만 부를 것.
		_pathFinder는 탐색 버퍼를 재사용하므로 스레드 안전하지 않다.
	-----------*/

	bool	LoadLevel();

	// 출발지와 목적지가 막힌 타일이면 가장 가까운 통행 가능 타일로 스냅한다.
	// outStart / outGoal에 실제로 사용된 타일이 담긴다.
	bool	FindPathToObject(TilePos start, uint64 targetObjectId,
							 OUT Vector<TilePos>& outPath, OUT TilePos& outStart, OUT TilePos& outGoal);

	uint64	FindFirstPlayerId();

	// 디버그 명령이 만든 더미 플레이어만 전부 걷어낸다. 접속 중인 클라는 건드리지 않는다.
	int32	RemoveAllDummies();

	/*----------
		충돌 판정

		전부 룸 잡 큐 안에서만 부를 것. 트리는 스레드 안전하지 않다.

		트리는 Tick 맨 앞에서 다시 만들어지므로, 질의 결과는 그 틱 안에서만 유효하다.
		틱 밖에서(예: 디버그 명령) 질의하려면 RebuildCollisionTree 를 먼저 부를 것.
	-----------*/

	void	RebuildCollisionTree();
	void	QueryRange(const Bounds& range, OUT Vector<GameObject*>& out);
	void	QueryCircle(int32 centerX, int32 centerY, int32 radius, OUT Vector<GameObject*>& out);

	/*----------
		AI

		전부 룸 잡 큐 안에서만 부를 것.
	-----------*/

	BehaviorTreeManager&	GetBtManager() { return _btManager; }

	// 트리를 다시 읽고, 붙어 있던 인스턴스를 새 트리로 다시 묶는다.
	// 재로드는 트리 주소도 슬롯 구성도 바꾸므로 다시 묶지 않으면 대롱거린다.
	bool		ReloadBehaviorTree(const string& name);

	bool		AttachBehavior(uint64 objectId, const string& treeName);
	bool		DetachBehavior(uint64 objectId);
	BtInstance*	FindBehavior(uint64 objectId);

	// 자동 틱을 끄면 Tick 이 AI 를 건드리지 않는다.
	// bt step 으로 한 틱씩 재현하며 볼 때 켜져 있으면 상태가 계속 굴러가 관찰이 안 된다.
	void		SetBehaviorAutoTick(bool enabled)	{ _behaviorAutoTick = enabled; }
	bool		IsBehaviorAutoTick() const			{ return _behaviorAutoTick; }

	// 붙어 있는 인스턴스를 한 틱 돌린다. Tick 이 부르고, 디버그 명령도 부른다.
	void		TickBehaviors(float deltaTime);
	BtStatus	TickBehavior(uint64 objectId, float deltaTime);

	float		GetTickDeltaTime() const { return static_cast<float>(TICK_INTERVAL_MS) / 1000.0f; }

	// 쿼드트리 결과를 대조하기 위한 전수 조사. 디버그 명령 전용이다.
	void	QueryCircleBruteForce(int32 centerX, int32 centerY, int32 radius,
								  OUT Vector<GameObject*>& out);

	/* Debug : 콘솔 / Admin 명령이 사용한다 */
	std::wstring	DescribeObjects();
	std::wstring	DescribeLevel();
	std::wstring	DescribePath(const Vector<TilePos>& path, TilePos start, TilePos goal);
	std::wstring	DescribeTree() const		{ return _collisionTree.Describe(); }
	int32			GetTreeNodeCount() const	{ return _collisionTree.GetNodeCount(); }
	int32			GetLastVisitedNodes() const	{ return _collisionTree.GetLastVisitedNodes(); }
	int32			GetLastExpandedCount() const { return _pathFinder.GetLastExpandedCount(); }
	int32			GetLastOpenedCount() const
	{
		return static_cast<int32>(_pathFinder.GetLastOpenedJumpPoints().size());
	}
	int32			GetLastPathJumpPointCount() const
	{
		return static_cast<int32>(_pathFinder.GetLastPathJumpPoints().size());
	}

private:
	// 갓 입장한 플레이어에게 룸 전체 스냅샷을 보낸다.
	void	SendEnterRoom(shared_ptr<Player> player);

	/*----------
		이동 내부 구현
	-----------*/

	// 이번 틱, 움직이는 모든 액터의 위치를 적분한다. Tick() 이 부른다.
	// 스텝은 고정 TICK_INTERVAL_MS 로 정수 계산한다 (float 미사용).
	void	UpdateMovement();

	// dirty 표시된 액터를 S_MOVE 한 방에 묶어 브로드캐스트한다.
	void	BroadcastMoves();

	// 수명이 다했거나 벽에 막힌 투사체를 걷어낸다. Tick() 이 이동 브로드캐스트 뒤에 부른다.
	void	SweepExpiredProjectiles();

	// 고정소수점 위치에 step 을 더하되 벽을 뚫지 않는다. 축을 분리해 슬라이드하고
	// 코너컷은 막는다. 액터-액터 충돌은 이번 범위 밖.
	// 반환값 : 복제 셀(_pos)이 바뀌었으면 true.
	bool	IntegrateActor(GameObject* object, int32 stepX, int32 stepY);

	// 8방향 enum -> 정수 단위 벡터.
	static void					DirUnit(Protocol::DirectionType dir, OUT int32& ux, OUT int32& uy);
	// 두 셀의 부호 차이 -> 8방향 enum.
	static Protocol::DirectionType	DirTo(const Protocol::Vector2& from, const Protocol::Vector2& to);

private:
	HashMap<uint64, GameObjectRef> _objects;

	Level			_level;
	BehaviorTreeManager	_btManager;

	// Debug 발판 : Monster 가 생기면 Monster 가 BtInstance 를 직접 소유하게 된다.
	// 지금은 몬스터가 없어 더미 플레이어에 붙여 검증한다.
	HashMap<uint64, BtInstance> _behaviors;
	bool						_behaviorAutoTick = true;

	JpsPathFinder	_pathFinder;
	QuadTree		_collisionTree;

	// Tick() 에서만 증가하지만 C_PING 핸들러(IOCP 워커)가 읽으므로 atomic.
	Atomic<uint64>	_tickCount = 0;
	uint64			_lastKeyframeTick = 0;
};

// Room은 StlAllocator 기반 컨테이너를 들고 있어서 GMemory보다 먼저 만들어지면 안 된다.
// (GMemory를 만드는 GCoreGlobal은 ServerCore 쪽 다른 TU의 정적 객체라 초기화 순서가 보장되지 않는다)
// 그래서 여기서는 선언만 하고 실제 생성은 main()에서 한다.
extern shared_ptr<Room> GRoom;
