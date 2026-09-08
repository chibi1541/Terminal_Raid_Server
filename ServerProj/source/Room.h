#pragma once
#include "JobQueue.h"
#include "Game/GameObject.h"
#include "Game/Level.h"
#include "Game/JpsPathFinder.h"
#include "Game/AStarPathFinder.h"
#include "Game/QuadTree.h"
#include "Game/MonsterSpawner.h"
#include "AI/BehaviorTreeManager.h"
#include "AI/BtInstance.h"

class Player;
class GameSession;

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
		SNAP_MAX_RADIUS			= 32,	// 막힌 셀을 통행 가능한 셀로 스냅할 때의 최대 반경(셀). 큰 박스(necro 24)가 벽에 붙어도 유효 중심을 찾도록 여유
		PATH_ARRIVE_TOL_CELLS	= 2,	// 경로 웨이포인트(점프 포인트) 도착 허용오차(셀). 틱당 이동량(~1.5셀)보다 커야 지나침에도 pathIndex 가 따라간다
		MOVE_KEYFRAME_INTERVAL	= 10,	// 이 틱마다 움직이는 액터를 강제로 브로드캐스트 (500ms 드리프트 보정)
		PROJECTILE_LIFETIME_TICKS	= 100,	// 투사체 기본 수명 (5초 @ 20Hz)
		MAX_CATCHUP_TICKS		= 5,	// 이 배수(5틱=250ms)만큼 밀리면 따라잡기 포기하고 리셋

		// 클라 이동 입력 검증(anti-cheat) + 재조정. HandleMove 가 클라 주장 시간 vs 서버 실측 시간을 대조한다.
		MOVE_JITTER_MARGIN_MS	= 200,	// 편도 지연 지터 허용치. heldMs 는 [실측-이값, 실측+이값] 으로 클램프
		MOVE_ABUSE_THRESHOLD_MS	= 3000,	// moveTimeCreditMs 누적이 이 값을 넘으면 어뷰징으로 강한 경고
		// 충돌 슬라이드 조각 크기는 MoveMath::SUBSTEP_SUBUNITS (Shared/MovementMath.h) 에서 온다.
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
	void	HandleMove(GameObjectRef object, uint32 inputSeq, uint32 clientTimeMs, int32 dir);

	// 플레이어 공격(발사). C_ATTACK 핸들러가 DoAsync 로 넘긴다.
	// aimCell/muzzleCell 은 클라가 화면->월드 변환해 보낸 값. 서버가 쿨다운·거리 검증 후 스폰.
	void	HandleAttack(GameObjectRef object, Protocol::Vector2 aimCell,
						 Protocol::Vector2 muzzleCell, uint32 clientTimeMs);

	// 목표 셀까지 JPS 경로를 깔고 추종 시작. 디버그 goto / 향후 AI 리프가 공유한다.
	bool	OrderMoveTo(uint64 objectId, int32 cellX, int32 cellY);

	// 8방향 직진 투사체 (proj 디버그 명령). cellsPerSec <= 0 이면 기본 속도.
	// ownerId 를 몬스터 id 로 주면 "몬스터 투사체"가 되어 플레이어를 맞힌다 (proj [ownerId]).
	GameObjectRef	SpawnProjectile(int32 cellX, int32 cellY, Protocol::DirectionType dir,
									int32 cellsPerSec, int32 lifetimeTicks,
									uint64 ownerId, int32 damage, Protocol::ProjectileType type);

	// 임의 각도 투사체. spawnFp = 고정소수점 스폰 위치, velSub = 서브유닛/초 속도 벡터.
	GameObjectRef	SpawnProjectileVec(int32 spawnFpX, int32 spawnFpY,
									   int32 velSubX, int32 velSubY, uint64 ownerId,
									   int32 rangeCells, int32 lifetimeTicks, int32 damage,
									   Protocol::ProjectileType type);

	// (originCellX, originCellY) 에서 (dirX, dirY) 방향(내부에서 정규화)으로 투사체 하나 발사.
	// 속도/사거리/피해/수명은 ProjectileData(type) 에서 파생. 몬스터 AI 리프(FireRadialBurst)가 쓴다.
	// HandleAttack 의 스폰 산수와 같은 식.
	void			SpawnProjectileAimed(uint64 ownerId, int32 originCellX, int32 originCellY,
										 float dirX, float dirY, Protocol::ProjectileType type);

	// 디버그 : 이동 루프만 count 틱 수동으로 굴린다. (bt step 과 같은 방식)
	void	DebugStepMovement(int32 count);

	/*----------
		디버그 오버레이 (충돌 격자 + 길찾기)

		C_DEBUG_CONFIG 로 켠 세션에게만 흐른다. 안 켜면 비용 0.
	-----------*/

	// wantLevelGrid 를 켠 세션에게 현재 충돌 격자를 1회 보낸다. IOCP 워커가 DoAsync 로 넘긴다.
	void	SendDebugLevelTo(shared_ptr<GameSession> session);

	// 마지막 틱의 충돌 처리 타이밍 (us). collision 콘솔 명령이 읽는다.
	uint32	GetLastTreeBuildMicros() const	{ return _lastTreeBuildMicros; }
	uint32	GetLastCollisionMicros() const	{ return _lastCollisionMicros; }

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

	// 사망한 플레이어를 원래 스폰 위치에 풀피로 되살리고 S_RESPAWN 브로드캐스트.
	// C_RESPAWN 이 룸 잡 큐로 넘긴다. 살아있는 개체가 온 요청은 무시한다.
	void	HandleRespawn(GameObjectRef object);

	uint32	GetWidth() const	{ return static_cast<uint32>(_level.GetWidth()); }
	uint32	GetHeight() const	{ return static_cast<uint32>(_level.GetHeight()); }

	const Level& GetLevel() const { return _level; }

	/*----------
		길찾기

		전부 룸 잡 큐 안에서만 부를 것.
		_pathFinder는 탐색 버퍼를 재사용하므로 스레드 안전하지 않다.
	-----------*/

	bool	LoadLevel();

	// (cellX, cellY) 에 한 변 boxCells 인 정사각 충돌 박스를 놓았을 때 벽에 안 걸리는가.
	// = 길찾기 NavGrid 통행 판정 = 이동 충돌(IsActorBoxBlocked) 과 동일. 스폰 위치 검증용.
	bool	IsBoxWalkable(int32 boxCells, int32 cellX, int32 cellY);

	// (cellX, cellY) 에서 boxCells 박스가 들어가는 가장 가까운 셀로 스냅 (in/out). maxRadiusCells 안에서 못 찾으면 false.
	bool	SnapCellForBox(int32 boxCells, int32 maxRadiusCells, int32& cellX, int32& cellY);

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

	// 더 이상 고정 50ms 가 아니다 - 가장 최근 틱에서 실제로 측정된 경과 시간.
	float		GetTickDeltaTime() const { return static_cast<float>(_lastDeltaMs) / 1000.0f; }

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
	int32			GetLastExpandedCount() const { return _pathFinder->GetLastExpandedCount(); }
	int64			GetLastScannedCount() const { return _pathFinder->GetLastScannedCount(); }
	int32			GetLastOpenedCount() const
	{
		return static_cast<int32>(_pathFinder->GetLastOpenedNodes().size());
	}
	int32			GetLastPathJumpPointCount() const
	{
		return static_cast<int32>(_pathFinder->GetLastPathNodes().size());
	}
	uint32			GetLastPathMicros() const { return _lastPathMicros; }

	// 활성 길찾기 교체 / 조회. pathalgo 콘솔 명령이 쓴다.
	void			SetPathFinder(EPathFinder kind);
	EPathFinder		GetPathFinderKind() const { return _pathFinderKind; }
	const wchar_t*	GetPathFinderName() const { return _pathFinder->Name(); }

	// JPS vs A* 성능 비교. pathbench 콘솔 명령이 쓴다.
	std::wstring	BenchPath(TilePos start, TilePos goal, int32 boxCells, int32 iters);
	std::wstring	BenchPathRandom(int32 count, int32 boxCells);

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

	// Death 클립 재생을 마친(_deathDespawnTick 도달) 몬스터 시체를 룸에서 뺀다.
	void	SweepDeadMonsters();

	// 살아있는 투사체마다 발사자 진영 반대편(플레이어<->몬스터)을 쿼드트리로 찾아 명중 판정.
	// 명중 시 DealDamage + 투사체 MarkExpired. Tick() 이 UpdateMovement 뒤(2차 트리 재구축 후) 부른다.
	void	ResolveProjectileHits();

	// wantQuadtree 세션에게 쿼드트리 노드 + 타이밍을 보낸다. Tick 끝에서 ~5Hz.
	void	BroadcastDebugQuadtree();

	// 고정소수점 위치에 step 을 더하되 벽을 뚫지 않는다. 축을 분리해 슬라이드하고
	// 코너컷은 막는다. 액터-액터 충돌은 이번 범위 밖.
	// 반환값 : 복제 셀(_pos)이 바뀌었으면 true.
	bool	IntegrateActor(GameObject* object, int32 stepX, int32 stepY);

	// 정확 catch-up : object 의 anchor 로 되감은 뒤, dir 방향으로 heldMs 만큼의 변위를
	// (클라 replay 와 동일한 단일 청크 식으로 계산해) <=0.5셀 조각으로 나눠 충돌을 보며
	// 다시 적분한다. HandleMove 가 방향을 바꾸기 직전에 부른다.
	void	IntegrateHeld(GameObject* object, Protocol::DirectionType dir, int32 heldMs);

	// centerX/Y 를 중심으로 object 의 충돌 박스(셀 단위, GetCollisionCells*) 전체가 벽에 막혔는지 검사.
	// 충돌 박스 미설정(1x1) 객체는 셀 1칸만 본다 - 기존 동작 그대로.
	bool	IsActorBoxBlocked(const GameObject* object, int32 centerX, int32 centerY) const;

	// 길찾기 경로가 바뀐 오브젝트의 상태를 wantPaths 세션들에게 보낸다.
	// cleared = true 면 경로 종료 통지(waypoints 비움). includeSearchNodes 면 직전 JPS
	// 탐색의 open 점프 포인트를 함께 싣는다(goto/path 명령 직후에만 의미 있음).
	void	BroadcastDebugPath(GameObject* object, bool cleared, bool includeSearchNodes);

	// WantsPaths 구독 세션이 하나라도 있는가. (BroadcastDebugPath / OrderMoveTo 공용)
	bool	AnyPathSubscriber() const;

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

	JpsPathFinder	_jps;
	AStarPathFinder	_astar;
	IPathFinder*	_pathFinder = &_jps;	// 활성 길찾기. pathalgo 콘솔로 교체.
	EPathFinder		_pathFinderKind = EPathFinder::Jps;
	uint32			_lastPathMicros = 0;	// 마지막 OrderMoveTo 탐색 소요 (us). 디버그 오버레이용.

	QuadTree		_collisionTree;

	// 레벨 몬스터 배치 + 개체 수 유지. BeginPlay 에서 초기 스폰, Tick 끝에서 리젠 체크.
	MonsterSpawner	_monsterSpawner;

	// Tick() 에서만 증가하지만 C_PING 핸들러(IOCP 워커)가 읽으므로 atomic.
	Atomic<uint64>	_tickCount = 0;
	uint64			_lastKeyframeTick = 0;

	/*----------
		틱 드리프트 보정

		DoTimer 가 항상 고정 TICK_INTERVAL_MS 로 재예약하면 디스패치/큐잉 지연이
		누적만 되고 절대 줄어들지 않는다. 실제 경과 시간을 측정해 시뮬레이션에 쓰고,
		다음 예약은 이번 틱이 예정보다 얼마나 늦었는지를 빼서 정박자로 수렴시킨다.
	-----------*/

	uint64	_lastTickWallClockMs = 0;	// 지난 틱이 실제로 실행된 시각 (GetTickCount64())
	uint64	_nextTickScheduleMs = 0;	// 다음 틱이 "원래" 실행됐어야 할 절대 시각 - 보정 기준
	uint32	_lastDeltaMs = TICK_INTERVAL_MS;	// 가장 최근 틱의 실제 경과 시간(ms)

	// 충돌 디버그 타이밍 (us). 2차 RebuildCollisionTree / ResolveProjectileHits 소요.
	uint32	_lastTreeBuildMicros = 0;
	uint32	_lastCollisionMicros = 0;
};

// Room은 StlAllocator 기반 컨테이너를 들고 있어서 GMemory보다 먼저 만들어지면 안 된다.
// (GMemory를 만드는 GCoreGlobal은 ServerCore 쪽 다른 TU의 정적 객체라 초기화 순서가 보장되지 않는다)
// 그래서 여기서는 선언만 하고 실제 생성은 main()에서 한다.
extern shared_ptr<Room> GRoom;
