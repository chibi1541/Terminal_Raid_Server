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
		TICK_INTERVAL_MS	= 50,
		SPAWN_MAX_TRY		= 64,	// 통행 가능한 셀을 무작위로 찾을 때의 시도 횟수
		SNAP_MAX_RADIUS		= 8,	// 막힌 타일을 통행 가능한 타일로 스냅할 때의 최대 반경
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

	Protocol::Vector2	FindSpawnPos();
	GameObjectRef		Find(uint64 objectId);

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
};

// Room은 StlAllocator 기반 컨테이너를 들고 있어서 GMemory보다 먼저 만들어지면 안 된다.
// (GMemory를 만드는 GCoreGlobal은 ServerCore 쪽 다른 TU의 정적 객체라 초기화 순서가 보장되지 않는다)
// 그래서 여기서는 선언만 하고 실제 생성은 main()에서 한다.
extern shared_ptr<Room> GRoom;
