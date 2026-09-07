#include "pch.h"
#include "Room.h"
#include "Protocol/ClientPacketHandler.h"
#include "Game/Player.h"
#include "Game/Monster.h"
#include "Game/Projectile.h"
#include "Game/ObjectIdGenerator.h"
#include "GameSession.h"
#include "Game/ProjectileData.h"
#include "Game/MonsterData.h"
#include "Game/CharacterData.h"
#include <algorithm>
#include <chrono>
#include <cmath>

// 생성은 main()에서 한다. Room.h의 주석 참고.
shared_ptr<Room> GRoom;

Room::Room()
{
}

Room::~Room()
{
}

void Room::BeginPlay()
{
	LoadLevel();

	const uint64 now = ::GetTickCount64();
	_lastTickWallClockMs = now;
	_nextTickScheduleMs = now + TICK_INTERVAL_MS;

	// 레벨 몬스터 배치 : 보스 + 사분면 좀비 무리. (플레이어 접속 전이라 S_SPAWN 은 아무에게도
	// 안 가지만, 이후 입장하는 클라는 SendEnterRoom 스냅샷으로 전부 받는다)
	_monsterSpawner.SetRoom(this);
	_monsterSpawner.SpawnInitial();

	DoTimer(TICK_INTERVAL_MS, &Room::Tick);

	LOG_INFO(L"[room] begin play (%u x %u)", GetWidth(), GetHeight());
}

void Room::Tick()
{
	_tickCount++;

	const uint64 now = ::GetTickCount64();

	// 이번 틱이 실제로 몇 ms 만에 돌아왔는지 측정한다. DoTimer 재예약이 항상
	// 고정 TICK_INTERVAL_MS 를 가정하면 디스패치/큐잉 지연이 누적만 되므로,
	// 시뮬레이션(이동 적분/AI 델타)은 이 실측값을 쓴다.
	uint64 deltaMs64 = now - _lastTickWallClockMs;
	if (deltaMs64 == 0)
		deltaMs64 = 1;	// 0 나눗셈/이동 정지 방지
	_lastTickWallClockMs = now;

	const uint64 maxDeltaMs = static_cast<uint64>(TICK_INTERVAL_MS) * MAX_CATCHUP_TICKS;
	if (deltaMs64 > maxDeltaMs)
		deltaMs64 = maxDeltaMs;	// 디버거/GC 정지급 지연은 시뮬레이션이 한 번에 확 전진하지 않게 자른다

	_lastDeltaMs = static_cast<uint32>(deltaMs64);

	// 이번 틱에 쓸 공간 인덱스를 먼저 세운다.
	// 이 아래에서 도는 이동 / 전투 로직은 전부 이 트리를 보게 된다.
	RebuildCollisionTree();

	// 붙어 있는 AI 를 돌린다. 공간 인덱스를 세운 뒤라 리프가 질의를 쓸 수 있다.
	// AI 는 여기서 의도(방향 / 경로)만 액터에 기록한다.
	TickBehaviors(GetTickDeltaTime());

	// 기록된 의도대로 위치를 적분한다.
	UpdateMovement();

	// 이동이 반영된 위치로 트리를 한 번 더 짓고, 투사체 명중 판정을 돌린다.
	// 두 구간 소요를 재서 디버그 오버레이(S_DEBUG_QUADTREE) / collision 명령이 쓴다.
	{
		const auto tBuild0 = std::chrono::steady_clock::now();
		RebuildCollisionTree();
		const auto tBuild1 = std::chrono::steady_clock::now();
		ResolveProjectileHits();
		const auto tHit1 = std::chrono::steady_clock::now();

		_lastTreeBuildMicros = static_cast<uint32>(
			std::chrono::duration_cast<std::chrono::microseconds>(tBuild1 - tBuild0).count());
		_lastCollisionMicros = static_cast<uint32>(
			std::chrono::duration_cast<std::chrono::microseconds>(tHit1 - tBuild1).count());
	}

	// 바뀐 액터를 한 번에 브로드캐스트한다.
	BroadcastMoves();

	// 수명이 다했거나 벽에 박힌(또는 이번 틱에 명중한) 투사체를 정리한다. (마지막 위치는 위에서 이미 보냈다)
	SweepExpiredProjectiles();

	// Death 클립 재생을 마친 몬스터 시체를 룸에서 뺀다.
	SweepDeadMonsters();

	// 좀비 수가 절반 이하로 줄었으면 인터벌마다 1 마리씩 보충한다.
	// (Enter/AttachBehavior 로 _objects/_behaviors 를 건드리므로 이번 틱의 순회가 전부 끝난 뒤에 부른다)
	_monsterSpawner.Tick(GetTickDeltaTime());

	// 쿼드트리 디버그 오버레이 (~5Hz, 구독 세션 없으면 비용 0).
	BroadcastDebugQuadtree();

	// 드리프트 보정 : 이번 틱이 예정 시각(_nextTickScheduleMs)보다 늦게 실행됐으면
	// 늦은 만큼 다음 예약 지연에서 빼서 정박자로 수렴시킨다. 너무 많이 밀렸으면
	// (디버거/GC 정지급) 따라잡기를 포기하고 지금 시각 기준으로 다시 잡는다.
	uint64 nextDelay = TICK_INTERVAL_MS;

	if (now >= _nextTickScheduleMs)
	{
		const uint64 behindMs = now - _nextTickScheduleMs;

		if (behindMs > maxDeltaMs)
			_nextTickScheduleMs = now;	// 포기 - 정박자를 지금부터 다시 잡는다
		else
			nextDelay = (behindMs < TICK_INTERVAL_MS) ? (TICK_INTERVAL_MS - behindMs) : 0;
	}

	_nextTickScheduleMs += TICK_INTERVAL_MS;

	// (jobs 명령의 reserved timers가 0이면 틱 루프가 끊긴 것)
	DoTimer(static_cast<uint32>(nextDelay), &Room::Tick);
}

void Room::Enter(GameObjectRef object, bool useRandomSpawnPos)
{
	if (object == nullptr)
		return;

	if (object->GetObjId() == 0)
		object->SetObjId(ObjectIdGenerator::GenerateObjectId(object->GetObjType()));

	if (_objects.find(object->GetObjId()) != _objects.end())
	{
		// 중복 입장. C_ENTER_ROOM을 두 번 보낸 경우 여기서 걸린다.
		LOG_WARN(L"[room] duplicated enter objectId=%llu", object->GetObjId());
		return;
	}

	if (useRandomSpawnPos)
	{
		// 실제 접속한 플레이어는 왼쪽 게이트 앞에서 시작한다.
		// 세션 없는 더미(디버그 spawn / spawnmany)는 종전대로 무작위 산개.
		const bool realPlayer = (object->GetObjType() == Protocol::OBJECT_PLAYER) &&
			(static_pointer_cast<Player>(object)->GetSession() != nullptr);

		object->SetPos(realPlayer ? _monsterSpawner.GetPlayerStartPos() : FindSpawnPos());
	}

	object->SetRoom(static_pointer_cast<Room>(shared_from_this()));

	// 목록에 넣기 '전에' 알린다.
	// 이렇게 하면 S_SPAWN이 본인에게는 가지 않고,
	// 본인은 바로 아래 S_ENTER_ROOM으로 전체 목록을 한 번에 받는다.
	{
		Protocol::S_SPAWN spawnPkt;
		object->FillObjectInfo(spawnPkt.add_objects());
		spawnPkt.set_servertick(static_cast<uint32>(_tickCount));
		Broadcast(ClientPacketHandler::MakeSendBuffer(spawnPkt), 0);
	}

	_objects[object->GetObjId()] = object;

	// 플레이어라면 본인에게 룸 전체 스냅샷을 보낸다.
	if (object->GetObjType() == Protocol::OBJECT_PLAYER)
		SendEnterRoom(static_pointer_cast<Player>(object));

	LOG_INFO(L"[room] enter objectId=%llu type=%d pos=(%d, %d) total=%d",
		object->GetObjId(), static_cast<int32>(object->GetObjType()),
		object->GetPosX(), object->GetPosY(), static_cast<int32>(_objects.size()));
}

void Room::Leave(GameObjectRef object)
{
	if (object == nullptr)
		return;

	if (_objects.erase(object->GetObjId()) == 0)
		return;

	object->ClearRoom();

	Protocol::S_DESPAWN despawnPkt;
	despawnPkt.add_objectids(object->GetObjId());
	Broadcast(ClientPacketHandler::MakeSendBuffer(despawnPkt), 0);

	LOG_INFO(L"[room] leave objectId=%llu total=%d",
		object->GetObjId(), static_cast<int32>(_objects.size()));
}

void Room::Broadcast(SendBufferRef sendBuffer, uint64 exceptId)
{
	if (sendBuffer == nullptr)
		return;

	for (auto& item : _objects)
	{
		if (item.first == exceptId)
			continue;

		if (item.second->GetObjType() != Protocol::OBJECT_PLAYER)
			continue;

		Player* player = static_cast<Player*>(item.second.get());

		// 디버그 더미 플레이어이거나 이미 끊긴 세션이면 건너뛴다.
		GameSessionRef session = player->GetSession();
		if (session == nullptr)
			continue;

		session->Send(sendBuffer);
	}
}

void Room::SendEnterRoom(shared_ptr<Player> player)
{
	GameSessionRef session = player->GetSession();
	if (session == nullptr)
		return;	// 디버그 더미 플레이어는 받을 대상이 없다

	Protocol::S_ENTER_ROOM enterPkt;
	enterPkt.set_success(true);
	enterPkt.set_width(GetWidth());
	enterPkt.set_height(GetHeight());
	enterPkt.set_servertick(static_cast<uint32>(_tickCount));
	player->FillObjectInfo(enterPkt.mutable_myobject());

	for (auto& item : _objects)
	{
		if (item.first == player->GetObjId())
			continue;

		item.second->FillObjectInfo(enterPkt.add_objects());
	}

	session->Send(ClientPacketHandler::MakeSendBuffer(enterPkt));
}

Protocol::Vector2 Room::FindSpawnPos()
{
	// RandomRange32의 난수 엔진은 락 없는 전역 static이다.
	// 룸 잡 큐 안에서만 부르기 때문에 직렬화가 보장된다. 다른 스레드에서 부르지 말 것.
	const int32 width = _level.GetWidth();
	const int32 height = _level.GetHeight();

	Protocol::Vector2 pos;

	for (int32 i = 0; i < SPAWN_MAX_TRY; i++)
	{
		const int32 x = RandomRange32(0, width - 1);
		const int32 y = RandomRange32(0, height - 1);

		if (_level.IsCellBlocked(x, y))
			continue;

		pos.set_x(x);
		pos.set_y(y);
		return pos;
	}

	// 무작위로 못 찾았으면 순차로 훑는다. 좁은 맵에서 무한 재시도로 빠지지 않게.
	for (int32 y = 0; y < height; y++)
	{
		for (int32 x = 0; x < width; x++)
		{
			if (_level.IsCellBlocked(x, y))
				continue;

			pos.set_x(x);
			pos.set_y(y);
			return pos;
		}
	}

	LOG_ERROR(L"[room] no walkable cell to spawn");
	return pos;
}

GameObjectRef Room::Find(uint64 objectId)
{
	auto findIt = _objects.find(objectId);
	if (findIt == _objects.end())
		return nullptr;

	return findIt->second;
}

std::wstring Room::DescribeObjects()
{
	WCHAR buffer[512];

	::swprintf_s(buffer, L"room %u x %u, objects %d",
		GetWidth(), GetHeight(), static_cast<int32>(_objects.size()));

	std::wstring result = buffer;

	for (auto& item : _objects)
	{
		GameObjectRef& object = item.second;

		const bool isPlayer = (object->GetObjType() == Protocol::OBJECT_PLAYER);
		Player* player = isPlayer ? static_cast<Player*>(object.get()) : nullptr;

		const bool isMonster = (object->GetObjType() == Protocol::OBJECT_MONSTER);
		Monster* monster = isMonster ? static_cast<Monster*>(object.get()) : nullptr;

		::swprintf_s(buffer, L"\n  id=%llu type=%d pos=(%d, %d) hp=%d/%d %hs%hs%hs",
			object->GetObjId(), static_cast<int32>(object->GetObjType()),
			object->GetPosX(), object->GetPosY(), object->GetHp(), object->GetMaxHp(),
			(player != nullptr) ? player->GetName().c_str() : "",
			(player != nullptr && player->IsDummy()) ? " [dummy]" : "",
			(monster != nullptr) ? monster->GetMonsterTypeName().c_str() : "");

		result += buffer;
	}

	return result;
}

/*----------------
	레벨 / 길찾기
-----------------*/

bool Room::LoadLevel()
{
	// Server.exe는 Binaries/Debug 에서 실행되므로 프로젝트 루트까지 두 단계 올라간다.
	static const WCHAR* LEVEL_PATH = L"../Data/Level01.xml";

	if (_level.LoadFromFile(LEVEL_PATH))
		return true;

	// 맵 파일 하나 때문에 서버가 안 뜨는 것보다는 빈 맵으로라도 뜨는 편이 낫다.
	_level.BuildEmpty(380, 280, 4);
	return false;
}

uint64 Room::FindFirstPlayerId()
{
	for (auto& item : _objects)
	{
		if (item.second->GetObjType() == Protocol::OBJECT_PLAYER)
			return item.first;
	}

	return 0;
}

bool Room::FindPathToObject(TilePos start, uint64 targetObjectId,
							OUT Vector<TilePos>& outPath, OUT TilePos& outStart, OUT TilePos& outGoal)
{
	outPath.clear();

	GameObjectRef target = Find(targetObjectId);
	if (target == nullptr)
		return false;

	const NavGrid& grid = _level.GetNavGrid();
	const TilePos rawGoal = grid.CellToTile(target->GetPosX(), target->GetPosY());

	// 타일 통행 판정이 보수적이라(3x3 전부 비어야 통행 가능) 플레이어가 벽 옆에 서 있기만 해도
	// 그 타일이 막힌 것으로 나온다. 그대로 실패시키면 알고리즘 버그로 오해하기 쉬우므로
	// 출발지와 목적지 양쪽 다 가장 가까운 통행 가능 타일로 스냅한다.
	if (grid.FindNearestWalkable(start, SNAP_MAX_RADIUS, OUT outStart) == false)
		return false;

	if (grid.FindNearestWalkable(rawGoal, SNAP_MAX_RADIUS, OUT outGoal) == false)
		return false;

	return _pathFinder.FindPath(grid, outStart, outGoal, OUT outPath);
}

/*---------------
	이동 (Movement)
----------------*/

void Room::DirUnit(Protocol::DirectionType dir, OUT int32& ux, OUT int32& uy)
{
	// y 는 아래로 증가한다 (레벨 0행이 위). UP = -y.
	switch (dir)
	{
	case Protocol::DIR_LEFT:		ux = -1; uy =  0; break;
	case Protocol::DIR_RIGHT:		ux =  1; uy =  0; break;
	case Protocol::DIR_UP:			ux =  0; uy = -1; break;
	case Protocol::DIR_DOWN:		ux =  0; uy =  1; break;
	case Protocol::DIR_UP_LEFT:		ux = -1; uy = -1; break;
	case Protocol::DIR_UP_RIGHT:	ux =  1; uy = -1; break;
	case Protocol::DIR_DOWN_LEFT:	ux = -1; uy =  1; break;
	case Protocol::DIR_DOWN_RIGHT:	ux =  1; uy =  1; break;
	default:						ux =  0; uy =  0; break;
	}
}

Protocol::DirectionType Room::DirTo(const Protocol::Vector2& from, const Protocol::Vector2& to)
{
	const int32 dx = (to.x() > from.x()) - (to.x() < from.x());
	const int32 dy = (to.y() > from.y()) - (to.y() < from.y());

	if (dx < 0 && dy < 0) return Protocol::DIR_UP_LEFT;
	if (dx < 0 && dy > 0) return Protocol::DIR_DOWN_LEFT;
	if (dx < 0)           return Protocol::DIR_LEFT;
	if (dx > 0 && dy < 0) return Protocol::DIR_UP_RIGHT;
	if (dx > 0 && dy > 0) return Protocol::DIR_DOWN_RIGHT;
	if (dx > 0)           return Protocol::DIR_RIGHT;
	if (dy < 0)           return Protocol::DIR_UP;
	if (dy > 0)           return Protocol::DIR_DOWN;
	return Protocol::DIR_NONE;
}

bool Room::IntegrateActor(GameObject* object, int32 stepX, int32 stepY)
{
	MovementComponent& m = object->Movement();

	const int32 cx = object->GetPosX();
	const int32 cy = object->GetPosY();

	// 좌표 산수(슬라이드/코너컷)는 클라와 공유. 여기는 풋프린트 콜백만 엮는다.
	MoveMath::SlideStep(m.fpX, m.fpY, stepX, stepY,
		[&](int32 tx, int32 ty) { return IsActorBoxBlocked(object, tx, ty); });

	object->SyncCellFromFixed();

	return (object->GetPosX() != cx || object->GetPosY() != cy);
}

void Room::IntegrateHeld(GameObject* object, Protocol::DirectionType dir, int32 heldMs)
{
	if (heldMs <= 0)
		return;

	int32 ux = 0;
	int32 uy = 0;
	DirUnit(dir, OUT ux, OUT uy);
	if (ux == 0 && uy == 0)
		return;

	MovementComponent& m = object->Movement();

	// 매 틱 free-run 이 밀어 놓은 위치를 버리고 anchor 로 되감은 뒤,
	// 클라 replay 와 같은 공유 적분기로 heldMs 만큼 정확히 다시 적분한다.
	m.fpX = m.anchorFpX;
	m.fpY = m.anchorFpY;

	MoveMath::IntegrateSlide(m.fpX, m.fpY, ux, uy, m.EffectiveSpeed(), heldMs,
		[&](int32 cx, int32 cy) { return IsActorBoxBlocked(object, cx, cy); });

	object->SyncCellFromFixed();
}

bool Room::IsActorBoxBlocked(const GameObject* object, int32 centerX, int32 centerY) const
{
	// 박스 산수는 클라 예측과 공유(MoveMath::BoxBlockedCells). 여기는 셀 판정만 엮는다.
	return MoveMath::BoxBlockedCells(centerX, centerY,
		object->GetCollisionCellsWide(), object->GetCollisionCellsHigh(),
		[&](int32 x, int32 y) { return _level.IsCellBlocked(x, y); });
}

void Room::UpdateMovement()
{
	const bool keyframe =
		(_tickCount - _lastKeyframeTick) >= static_cast<uint64>(MOVE_KEYFRAME_INTERVAL);

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();
		if (object == nullptr || object->IsAlive() == false)
			continue;

		// 피격 경직 중이면 적분 정지. dir 을 한 번 NONE 으로 알려 클라 보간이 외삽을 멈추게 한다.
		// state/path 는 유지 - 경직이 풀리면 경로 추종을 그대로 이어간다.
		if (object->IsStunned(_tickCount))
		{
			if (object->Movement().dir != Protocol::DIR_NONE)
			{
				object->Movement().dir = Protocol::DIR_NONE;
				object->Movement().dirty = true;
			}
			continue;
		}

		MovementComponent& m = object->Movement();
		if (m.state != MoveState::Moving)
			continue;

		// 경로 추종 : 현재 웨이포인트에 닿았으면 다음으로, dir 을 웨이포인트 방향에 맞춘다.
		if (m.HasPath())
		{
			// 이 경로는 이 액터의 풋프린트로 구운 NavGrid 기준으로 짜였다.
			// 웨이포인트 중심 좌표도 반드시 같은 grid 로 뽑아야 어긋나지 않는다.
			const NavGrid& grid = _level.GetNavGridForFootprint(
				object->GetFootprintTilesWide(), object->GetFootprintTilesHigh());

			Protocol::Vector2 wp = grid.TileToCellCenter(m.path[m.pathIndex]);

			if (object->GetPosX() == wp.x() && object->GetPosY() == wp.y())
			{
				m.pathIndex++;

				if (m.HasPath() == false)
				{
					m.ClearPath();
					m.dir = Protocol::DIR_NONE;
					m.state = MoveState::Idle;
					m.dirty = true;
					BroadcastDebugPath(object, /*cleared*/ true, /*includeSearchNodes*/ false);
					continue;
				}

				wp = grid.TileToCellCenter(m.path[m.pathIndex]);

				// 웨이포인트 전진 - 디버그 오버레이의 "현재 목표"를 갱신한다. (매 틱이 아님)
				BroadcastDebugPath(object, /*cleared*/ false, /*includeSearchNodes*/ false);
			}

			const Protocol::DirectionType want = DirTo(object->GetPos(), wp);
			if (want != m.dir)
			{
				m.dir = want;
				m.dirty = true;
			}
		}

		// 임의 각도 속도 벡터 (투사체) - 8방향 dir 대신 이걸로 적분한다.
		if (m.velSubX != 0 || m.velSubY != 0)
		{
			const int32 beforeCX = object->GetPosX();
			const int32 beforeCY = object->GetPosY();

			Projectile* proj = (object->GetObjType() == Protocol::OBJECT_PROJECTILE)
				? static_cast<Projectile*>(object) : nullptr;
			const bool ignoreWalls = (proj != nullptr) && proj->IgnoresWalls();

			// 투사체는 슬라이드 안 함 - 막힌 셀 만나면 그 자리에서 멈추고 hitWall.
			// ignoreWalls 투사체는 벽 검사를 안 해 통과한다 (사거리/수명/월드 밖으로만 소멸).
			const bool hitWall = MoveMath::IntegrateVec(m.fpX, m.fpY, m.velSubX, m.velSubY,
				static_cast<int32>(_lastDeltaMs),
				[&](int32 cx, int32 cy) { return ignoreWalls ? false : _level.IsCellBlocked(cx, cy); });
			object->SyncCellFromFixed();

			if (proj != nullptr)
			{
				const int32 px = proj->GetPosX();
				const int32 py = proj->GetPosY();
				const bool outOfWorld = px < 0 || py < 0
					|| px >= _level.GetWidth() || py >= _level.GetHeight();

				if (hitWall || proj->IsOutOfRange() || outOfWorld)
					proj->MarkExpired();	// SweepExpiredProjectiles 가 이 틱 끝에 걷어간다
			}

			if (object->GetPosX() != beforeCX || object->GetPosY() != beforeCY || keyframe)
				m.dirty = true;

			continue;
		}

		int32 ux = 0;
		int32 uy = 0;
		DirUnit(m.dir, OUT ux, OUT uy);

		if (ux == 0 && uy == 0)
			continue;

		// 이번 틱 이동. 클라 예측(LocalPlayer::ReplayInputs)과 같은 공유 적분기를 쓴다.
		// 조각으로 나눠 슬라이드하므로 라그 틱(_lastDeltaMs 큼)에도 벽을 안 뚫는다.
		const int32 beforeCX = object->GetPosX();
		const int32 beforeCY = object->GetPosY();

		MoveMath::IntegrateSlide(m.fpX, m.fpY, ux, uy, m.EffectiveSpeed(),
			static_cast<int32>(_lastDeltaMs),
			[&](int32 cx, int32 cy) { return IsActorBoxBlocked(object, cx, cy); });
		object->SyncCellFromFixed();

		const bool cellChanged =
			(object->GetPosX() != beforeCX || object->GetPosY() != beforeCY);

		if (cellChanged || keyframe)
			m.dirty = true;
	}

	if (keyframe)
		_lastKeyframeTick = _tickCount;
}

void Room::BroadcastMoves()
{
	Protocol::S_MOVE pkt;

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();
		if (object == nullptr)
			continue;

		MovementComponent& m = object->Movement();
		if (m.dirty == false)
			continue;

		Protocol::MoveInfo* info = pkt.add_moves();
		info->set_objectid(object->GetObjId());
		info->mutable_pos()->CopyFrom(object->GetPos());
		info->set_dir(m.dir);
		info->set_speed(m.EffectiveSpeed());
		info->set_servertick(static_cast<uint32>(_tickCount));
		info->set_possubx(object->GetFixedX());	// 서브유닛 권위 위치. 원격 보간이 이 값을 쓴다.
		info->set_possuby(object->GetFixedY());

		m.dirty = false;
	}

	if (pkt.moves_size() == 0)
		return;

	pkt.set_servertick(static_cast<uint32>(_tickCount));
	pkt.set_deltams(_lastDeltaMs);
	pkt.set_servertime(_lastTickWallClockMs);
	Broadcast(ClientPacketHandler::MakeSendBuffer(pkt), 0);
}

void Room::HandleMove(GameObjectRef object, uint32 inputSeq, uint32 clientTimeMs, int32 dir)
{
	if (object == nullptr)
		return;

	MovementComponent& m = object->Movement();

	// 순서 역전 방어. 늦게 도착한 오래된 입력은 버린다. (seq 0 = 디버그 호출은 그냥 통과)
	if (inputSeq != 0 && inputSeq <= m.lastProcessedInputSeq)
		return;

	m.lastProcessedInputSeq = inputSeq;

	// 범위 밖 dir 은 정지로 취급한다.
	Protocol::DirectionType newDir = Protocol::DIR_NONE;
	if (dir >= Protocol::DIR_NONE && dir <= Protocol::DIR_DOWN_RIGHT)
		newDir = static_cast<Protocol::DirectionType>(dir);

	// 피격 경직 / 사망 중에는 입력을 무시하고 그 자리에 고정한다.
	// (catch-up 적분도 건너뛴다. 클라도 이 창 동안 예측을 멈추므로 위치가 일치한다.)
	const bool suppressed = (object->IsAlive() == false) || object->IsStunned(_tickCount);
	if (suppressed)
		newDir = Protocol::DIR_NONE;

	// --- 이동량 검증 + 정확 catch-up (reconciliation 앵커) ---
	//
	// 방향-홀드 모델에서 "직전 방향이 유지된 시간"을 두 관점으로 잰다.
	//  - 클라 주장 : clientTimeMs - 직전 입력의 clientTimeMs  (클라 단조 시계)
	//  - 서버 실측 : now - 직전 입력을 서버가 처리한 시각      (서버 단조 시계)
	// 서버 실측을 진실로 삼고, 클라 타임스탬프는 ±지터(MOVE_JITTER_MARGIN_MS) 범위에서만
	// 신뢰한다. 이렇게 클램프한 heldMs 로 직전 방향을 anchor 에서 정확히 다시 적분해
	// 권위 위치를 확정한다(매 틱 free-run 이 벌려 놓은 잔차는 여기서 사라진다).
	// 클라가 시간을 부풀리면 상한을 넘긴 만큼 moveTimeCreditMs 에 쌓고 경고한다.
	// (seq 0 = 디버그 호출은 클라 시계가 없으므로 건너뛴다)
	if (inputSeq != 0)
	{
		const uint64 now = ::GetTickCount64();

		if (m.hasInputTimeBase)
		{
			const uint32 heldMsClient = clientTimeMs - m.lastInputClientTimeMs;	// uint32 wrap-safe
			uint64 heldMsServerRaw = now - m.lastInputWallMs;
			if (heldMsServerRaw > 60000)
				heldMsServerRaw = 60000;										// 비정상 장기 공백 상한
			const int32 heldMsServer = static_cast<int32>(heldMsServerRaw);

			int32 lo = heldMsServer - MOVE_JITTER_MARGIN_MS;
			if (lo < 0)
				lo = 0;
			const int32 hi = heldMsServer + MOVE_JITTER_MARGIN_MS;

			int32 heldMs = static_cast<int32>(heldMsClient);
			if (heldMs < lo) heldMs = lo;
			if (heldMs > hi) heldMs = hi;

			if (static_cast<int32>(heldMsClient) > hi)
			{
				const int64 overshootMs = static_cast<int64>(heldMsClient) - hi;
				m.moveTimeCreditMs += overshootMs;

				LOG_WARN(L"[move-check] objectId=%llu 클라 주장 %ums > 허용 %dms (실측 %dms + 여유 %dms), 초과 %lldms 누적 %lldms",
					object->GetObjId(), heldMsClient, hi, heldMsServer,
					static_cast<int32>(MOVE_JITTER_MARGIN_MS), overshootMs, m.moveTimeCreditMs);

				if (m.moveTimeCreditMs > MOVE_ABUSE_THRESHOLD_MS)
				{
					LOG_WARN(L"[move-check] objectId=%llu 이동 시간 어뷰징 의심 - 누적 초과 %lldms (임계 %dms)",
						object->GetObjId(), m.moveTimeCreditMs, static_cast<int32>(MOVE_ABUSE_THRESHOLD_MS));
				}
			}
			else if (m.moveTimeCreditMs > 0)
			{
				// 정상 구간마다 그동안 쌓인 의심분을 서서히 돌려준다 (일시적 지터 오탐 방지).
				m.moveTimeCreditMs -= MOVE_JITTER_MARGIN_MS;
				if (m.moveTimeCreditMs < 0)
					m.moveTimeCreditMs = 0;
			}

			// 직전 방향(m.dir)을 anchor 에서 heldMs 만큼 정확히 적분 -> 권위 위치 확정.
			// 경직/사망 중이면 적분을 건너뛰어 그 자리에 고정.
			if (suppressed == false)
				IntegrateHeld(object.get(), m.dir, heldMs);
		}

		m.lastInputClientTimeMs = clientTimeMs;
		m.lastInputWallMs = now;
		m.hasInputTimeBase = true;
	}

	// 새 방향을 반영한다. anchor 는 방금 확정된 권위 위치(정확 catch-up 결과, 또는 첫 입력이면 현 위치).
	m.anchorFpX = m.fpX;
	m.anchorFpY = m.fpY;

	// 방향 입력이 오면 진행 중이던 경로 추종은 취소한다.
	const bool hadPath = m.HasPath();
	m.ClearPath();
	m.dir = newDir;
	m.state = (newDir == Protocol::DIR_NONE) ? MoveState::Idle : MoveState::Moving;
	m.dirty = true;

	if (hadPath)
		BroadcastDebugPath(object.get(), /*cleared*/ true, /*includeSearchNodes*/ false);

	// 이동을 요청한 플레이어에게 ack. 클라 재조정의 앵커.
	if (object->GetObjType() == Protocol::OBJECT_PLAYER)
	{
		Player* player = static_cast<Player*>(object.get());

		if (GameSessionRef session = player->GetSession())
		{
			Protocol::S_MOVE_ACK ack;
			ack.set_lastprocessedinputseq(m.lastProcessedInputSeq);
			ack.set_servertick(static_cast<uint32>(_tickCount));
			ack.mutable_pos()->CopyFrom(object->GetPos());
			ack.set_dir(m.dir);
			ack.set_possubx(object->GetFixedX());	// 서브유닛 권위 위치. 클라 재조정 앵커.
			ack.set_possuby(object->GetFixedY());
			session->Send(ClientPacketHandler::MakeSendBuffer(ack));
		}
	}
}

bool Room::OrderMoveTo(uint64 objectId, int32 cellX, int32 cellY)
{
	GameObjectRef object = Find(objectId);
	if (object == nullptr)
		return false;

	const NavGrid& grid = _level.GetNavGridForFootprint(
		object->GetFootprintTilesWide(), object->GetFootprintTilesHigh());

	const TilePos startTile = grid.CellToTile(object->GetPosX(), object->GetPosY());
	const TilePos goalTile  = grid.CellToTile(cellX, cellY);

	TilePos snappedStart;
	TilePos snappedGoal;
	if (grid.FindNearestWalkable(startTile, SNAP_MAX_RADIUS, OUT snappedStart) == false)
		return false;
	if (grid.FindNearestWalkable(goalTile, SNAP_MAX_RADIUS, OUT snappedGoal) == false)
		return false;

	MovementComponent& m = object->Movement();
	m.ClearPath();

	if (_pathFinder.FindPath(grid, snappedStart, snappedGoal, OUT m.path) == false || m.path.empty())
	{
		m.state = MoveState::Idle;
		m.dir = Protocol::DIR_NONE;
		return false;
	}

	// path[0] 은 출발 타일이다. 이미 그 근처이니 다음 타일부터 향한다.
	m.pathIndex = (m.path.size() > 1) ? 1 : 0;
	m.state = MoveState::Moving;
	m.dirty = true;

	// 방금 확정된 경로 + 이 탐색의 JPS open 노드를 디버그 오버레이로 보낸다.
	BroadcastDebugPath(object.get(), /*cleared*/ false, /*includeSearchNodes*/ true);
	return true;
}

void Room::SendDebugLevelTo(shared_ptr<GameSession> session)
{
	if (session == nullptr)
		return;

	const int32 width = _level.GetWidth();
	const int32 height = _level.GetHeight();
	const Vector<uint8>& cells = _level.GetCells();

	if (width <= 0 || height <= 0 ||
		static_cast<size_t>(width) * height != cells.size())
		return;

	// 서버 SendBuffer 청크(6000) / 클라 RecvBuffer(4096) 안에 들어가도록 행 밴드로 쪼갠다.
	// width=380 이면 행당 48바이트, 48행 = 2304바이트 (+ 헤더/프로토버프 오버헤드) < 4KB.
	const int32 bytesPerRow = (width + 7) / 8;
	const int32 rowsPerChunk = (bytesPerRow > 0) ? (std::max)(1, 3000 / bytesPerRow) : height;

	for (int32 startRow = 0; startRow < height; startRow += rowsPerChunk)
	{
		const int32 rowCount = (std::min)(rowsPerChunk, height - startRow);

		Protocol::S_DEBUG_LEVEL pkt;
		pkt.set_width(static_cast<uint32>(width));
		pkt.set_height(static_cast<uint32>(height));
		pkt.set_tilesize(static_cast<uint32>(_level.GetTileSize()));
		pkt.set_startrow(static_cast<uint32>(startRow));
		pkt.set_rowcount(static_cast<uint32>(rowCount));

		// 이 청크 로컬 비트. bit i = 셀 (startRow + i/width, i%width).
		const int64 chunkCells = static_cast<int64>(width) * rowCount;
		std::string bits(static_cast<size_t>((chunkCells + 7) / 8), '\0');
		for (int64 i = 0; i < chunkCells; ++i)
		{
			const size_t globalIndex = static_cast<size_t>(startRow) * width + i;
			if (cells[globalIndex] != 0)
				bits[i >> 3] |= static_cast<char>(1 << (i & 7));
		}
		pkt.set_blockedbits(std::move(bits));

		session->Send(ClientPacketHandler::MakeSendBuffer(pkt));
	}
}

void Room::BroadcastDebugPath(GameObject* object, bool cleared, bool includeSearchNodes)
{
	if (object == nullptr)
		return;

	// 구독 세션이 하나도 없으면 조립 비용조차 아낀다.
	bool anySubscriber = false;
	for (auto& item : _objects)
	{
		GameObject* o = item.second.get();
		if (o == nullptr || o->GetObjType() != Protocol::OBJECT_PLAYER)
			continue;
		if (GameSessionRef s = static_cast<Player*>(o)->GetSession())
		{
			if (s->WantsPaths())
			{
				anySubscriber = true;
				break;
			}
		}
	}
	if (anySubscriber == false)
		return;

	const MovementComponent& m = object->Movement();
	const NavGrid& grid = _level.GetNavGridForFootprint(
		object->GetFootprintTilesWide(), object->GetFootprintTilesHigh());

	Protocol::S_DEBUG_PATH pkt;
	pkt.set_objectid(object->GetObjId());
	pkt.set_cleared(cleared);
	pkt.set_currentindex(static_cast<uint32>(m.pathIndex));

	if (cleared == false)
	{
		for (const TilePos& tile : m.path)
		{
			const Protocol::Vector2 c = grid.TileToCellCenter(tile);
			Protocol::Vector2* out = pkt.add_waypoints()->mutable_cell();
			out->set_x(c.x());
			out->set_y(c.y());
		}
	}

	if (includeSearchNodes)
	{
		// 청크 제한(클라 RecvBuffer 4096) 안에 들어가도록 상한을 둔다.
		int32 emitted = 0;
		for (const TilePos& tile : _pathFinder.GetLastOpenedJumpPoints())
		{
			if (emitted++ >= 400)
				break;
			const Protocol::Vector2 c = grid.TileToCellCenter(tile);
			Protocol::Vector2* out = pkt.add_searchnodes()->mutable_cell();
			out->set_x(c.x());
			out->set_y(c.y());
		}
	}

	const SendBufferRef buffer = ClientPacketHandler::MakeSendBuffer(pkt);
	for (auto& item : _objects)
	{
		GameObject* o = item.second.get();
		if (o == nullptr || o->GetObjType() != Protocol::OBJECT_PLAYER)
			continue;
		if (GameSessionRef s = static_cast<Player*>(o)->GetSession())
		{
			if (s->WantsPaths())
				s->Send(buffer);
		}
	}
}

void Room::DebugStepMovement(int32 count)
{
	for (int32 i = 0; i < count; i++)
	{
		_tickCount++;
		UpdateMovement();
		BroadcastMoves();
	}
}

/*---------------
	이벤트성 상태 (Hit / Death / Attack)
----------------*/

bool Room::DealDamage(uint64 attackerId, uint64 targetId, int32 damage)
{
	GameObjectRef target = Find(targetId);
	if (target == nullptr || target->IsAlive() == false)
		return false;

	const bool died = target->ApplyDamage(damage);

	// 피격 경직 시간 (타입별 데이터). 죽었으면 경직 대신 사망 처리로 넘어간다.
	int32 stunMs = 0;
	if (died == false)
	{
		if (target->GetObjType() == Protocol::OBJECT_MONSTER)
			stunMs = MonsterData::Get().Find(
				static_cast<Monster*>(target.get())->GetMonsterType()).hitStunMs;
		else if (target->GetObjType() == Protocol::OBJECT_PLAYER)
			stunMs = CharacterData::Get().Find(
				static_cast<Player*>(target.get())->GetCharacterType()).hitStunMs;

		if (stunMs > 0)
		{
			const uint64 stunTicks =
				(static_cast<uint64>(stunMs) + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS;
			target->SetStunUntilTick(_tickCount + stunTicks);
		}
	}

	Protocol::S_HIT hitPkt;
	hitPkt.set_targetid(targetId);
	hitPkt.set_attackerid(attackerId);
	hitPkt.set_damage(damage);
	hitPkt.set_newhp(target->GetHp());
	hitPkt.set_servertick(static_cast<uint32>(_tickCount));
	hitPkt.set_stunms(static_cast<uint32>(stunMs));
	Broadcast(ClientPacketHandler::MakeSendBuffer(hitPkt), 0);

	if (died)
	{
		Protocol::S_DEATH deathPkt;
		deathPkt.set_objectid(targetId);
		deathPkt.set_killerid(attackerId);
		deathPkt.set_servertick(static_cast<uint32>(_tickCount));
		Broadcast(ClientPacketHandler::MakeSendBuffer(deathPkt), 0);

		if (target->GetObjType() == Protocol::OBJECT_PLAYER)
		{
			// 게임 오버는 나중. 룸에 시체로 남긴다 (IsAlive()==false 라 판정/트리 제외, 입력은 클라가 막음).
		}
		else if (target->GetObjType() == Protocol::OBJECT_MONSTER)
		{
			Monster* monster = static_cast<Monster*>(target.get());
			const int32 fadeMs = MonsterData::Get().Find(monster->GetMonsterType()).deathFadeMs;

			if (fadeMs > 0)
			{
				// Death 클립 재생을 기다렸다 Tick 이 SweepDeadMonsters 로 뺀다.
				const uint64 fadeTicks =
					(static_cast<uint64>(fadeMs) + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS;
				monster->SetDeathDespawnTick(_tickCount + fadeTicks);
			}
			else
			{
				Leave(target);	// 보스 / 즉시 디스폰
			}
		}
		else
		{
			Leave(target);
		}
	}

	return true;
}

void Room::NotifyAttackStart(uint64 objectId, Protocol::DirectionType dir)
{
	Protocol::S_ATTACK_START pkt;
	pkt.set_objectid(objectId);
	pkt.set_dir(dir);
	pkt.set_servertick(static_cast<uint32>(_tickCount));
	Broadcast(ClientPacketHandler::MakeSendBuffer(pkt), 0);
}

GameObjectRef Room::SpawnProjectile(int32 cellX, int32 cellY, Protocol::DirectionType dir,
								   int32 cellsPerSec, int32 lifetimeTicks,
								   uint64 ownerId, int32 damage, Protocol::ProjectileType type)
{
	ProjectileRef proj = MakeShared<Projectile>();

	proj->SetProjectileType(type);
	proj->SetPos(cellX, cellY);
	proj->Launch(dir, cellsPerSec, _tickCount,
		(lifetimeTicks > 0) ? lifetimeTicks : PROJECTILE_LIFETIME_TICKS, ownerId, damage);

	// spawn 명령과 동일하게 좌표를 지정해서 넣는다 (랜덤 스폰 X).
	Enter(static_pointer_cast<GameObject>(proj), false);

	return proj;
}

GameObjectRef Room::SpawnProjectileVec(int32 spawnFpX, int32 spawnFpY,
									   int32 velSubX, int32 velSubY, uint64 ownerId,
									   int32 rangeCells, int32 lifetimeTicks, int32 damage,
									   Protocol::ProjectileType type)
{
	ProjectileRef proj = MakeShared<Projectile>();

	proj->SetProjectileType(type);
	proj->SetFixedPos(spawnFpX, spawnFpY);
	proj->LaunchVec(velSubX, velSubY, ownerId, rangeCells, _tickCount,
		(lifetimeTicks > 0) ? lifetimeTicks : PROJECTILE_LIFETIME_TICKS, damage);

	Enter(static_pointer_cast<GameObject>(proj), false);

	return proj;
}

void Room::SpawnProjectileAimed(uint64 ownerId, int32 originCellX, int32 originCellY,
								float dirX, float dirY, Protocol::ProjectileType type)
{
	const float len = ::sqrtf(dirX * dirX + dirY * dirY);
	if (len < 0.0001f)
		return;

	const float nx = dirX / len;
	const float ny = dirY / len;

	const ProjectileDef& def = ProjectileData::Get().Find(type);

	const float scale = static_cast<float>(MoveMath::POS_SCALE);

	// 스폰 위치 = 원점 + 방향 * spawnForwardCells (서브셀 정밀).
	const float ox = static_cast<float>(originCellX) + nx * def.spawnForwardCells;
	const float oy = static_cast<float>(originCellY) + ny * def.spawnForwardCells;
	const int32 spawnFpX = static_cast<int32>(ox * scale) + MoveMath::POS_SCALE / 2;
	const int32 spawnFpY = static_cast<int32>(oy * scale) + MoveMath::POS_SCALE / 2;

	const int32 velSubX = static_cast<int32>(nx * def.speedCellsPerSec * scale);
	const int32 velSubY = static_cast<int32>(ny * def.speedCellsPerSec * scale);

	// 사거리 -> 수명 틱 (HandleAttack 과 동일).
	const int32 lifetimeTicks = (def.speedCellsPerSec > 0)
		? static_cast<int32>((static_cast<int64>(def.rangeCells) * 1000
			/ def.speedCellsPerSec + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS) + 2
		: PROJECTILE_LIFETIME_TICKS;

	SpawnProjectileVec(spawnFpX, spawnFpY, velSubX, velSubY,
		ownerId, def.rangeCells, lifetimeTicks, def.damage, type);
}

void Room::HandleAttack(GameObjectRef object, Protocol::Vector2 aimCell,
						Protocol::Vector2 muzzleCell, uint32 clientTimeMs)
{
	(void)clientTimeMs;

	if (object == nullptr || object->GetObjType() != Protocol::OBJECT_PLAYER)
		return;

	// 피격 경직 / 사망 중에는 공격도 막는다.
	if (object->IsAlive() == false || object->IsStunned(_tickCount))
		return;

	Player* player = static_cast<Player*>(object.get());

	const ProjectileDef& proj = ProjectileData::Get().GetDefault();

	// 발사 쿨다운 (서버 강제).
	const uint64 now = ::GetTickCount64();
	if (player->GetLastAttackWallMs() != 0 &&
		now - player->GetLastAttackWallMs() < static_cast<uint64>(proj.fireIntervalMs))
	{
		return;
	}

	// muzzle 을 플레이어 권위 위치 근처로 클램프 (스폰 위치 조작 방지).
	// 클라는 이제 muzzle 로 몸통 중심(= 자기 위치)을 그대로 보낸다 - 회전 보정 없음.
	// 예측 오차만큼의 여유(kMuzzleMarginCells)만 허용한다.
	const float px = static_cast<float>(object->GetPosX());
	const float py = static_cast<float>(object->GetPosY());
	float mx = static_cast<float>(muzzleCell.x());
	float my = static_cast<float>(muzzleCell.y());

	const float mdx = mx - px;
	const float mdy = my - py;
	const float mDist = ::sqrtf(mdx * mdx + mdy * mdy);
	const float kMuzzleMarginCells = 4.0f;

	if (mDist > kMuzzleMarginCells && mDist > 0.001f)
	{
		mx = px + mdx / mDist * kMuzzleMarginCells;
		my = py + mdy / mDist * kMuzzleMarginCells;
	}

	// 조준 방향.
	const float adx = static_cast<float>(aimCell.x()) - mx;
	const float ady = static_cast<float>(aimCell.y()) - my;
	const float aDist = ::sqrtf(adx * adx + ady * ady);

	if (aDist < 1.0f)
		return;	// degenerate - 커서가 발사 기준점과 겹침

	const float ndx = adx / aDist;
	const float ndy = ady / aDist;

	// 스폰 위치 = muzzle + 조준 * spawnForwardCells, 서브셀 정밀.
	const float scale = static_cast<float>(MoveMath::POS_SCALE);
	const int32 spawnFpX = static_cast<int32>((mx + ndx * proj.spawnForwardCells) * scale)
		+ MoveMath::POS_SCALE / 2;
	const int32 spawnFpY = static_cast<int32>((my + ndy * proj.spawnForwardCells) * scale)
		+ MoveMath::POS_SCALE / 2;

	// 속도 벡터 (서브유닛/초).
	const int32 velSubX = static_cast<int32>(ndx * proj.speedCellsPerSec * scale);
	const int32 velSubY = static_cast<int32>(ndy * proj.speedCellsPerSec * scale);

	// 사정거리 -> 수명 틱 파생.
	const int32 lifetimeTicks = (proj.speedCellsPerSec > 0)
		? static_cast<int32>((static_cast<int64>(proj.rangeCells) * 1000
			/ proj.speedCellsPerSec + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS) + 2
		: PROJECTILE_LIFETIME_TICKS;

	SpawnProjectileVec(spawnFpX, spawnFpY, velSubX, velSubY,
		object->GetObjId(), proj.rangeCells, lifetimeTicks, proj.damage, proj.type);

	player->SetLastAttackWallMs(now);

	// 발사자 공격 모션 (8방향).
	Protocol::Vector2 from;
	from.set_x(static_cast<int32>(mx));
	from.set_y(static_cast<int32>(my));
	NotifyAttackStart(object->GetObjId(), DirTo(from, aimCell));
}

void Room::ResolveProjectileHits()
{
	// DealDamage 가 Leave 로 _objects 를 건드리므로, 판정은 먼저 다 모으고 나서 적용한다.
	struct Hit { uint64 attackerId; uint64 targetId; int32 damage; };
	Vector<Hit> hits;

	Vector<GameObject*> candidates;

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		if (object == nullptr || object->GetObjType() != Protocol::OBJECT_PROJECTILE)
			continue;
		if (object->IsAlive() == false)
			continue;

		Projectile* proj = static_cast<Projectile*>(object);

		// 발사자 진영 -> 맞힐 대상 타입. ownerId 0(디버그 스폰)은 아무도 안 맞힌다.
		const Protocol::ObjectType ownerType =
			ObjectIdGenerator::GetObjectType(proj->GetOwnerId());

		Protocol::ObjectType targetType;
		if (ownerType == Protocol::OBJECT_PLAYER)
			targetType = Protocol::OBJECT_MONSTER;
		else if (ownerType == Protocol::OBJECT_MONSTER)
			targetType = Protocol::OBJECT_PLAYER;
		else
			continue;

		// QueryCircle 은 (대상._radius + 질의반경) 겹침까지 이미 걸러 준다.
		// 질의반경 = 투사체 반경 => 결과 = 투사체 원과 겹치는 액터들.
		QueryCircle(proj->GetPosX(), proj->GetPosY(), proj->GetRadius(), OUT candidates);

		for (GameObject* c : candidates)
		{
			if (c == nullptr || c->GetObjId() == proj->GetOwnerId())
				continue;
			if (c->GetObjType() != targetType || c->IsAlive() == false)
				continue;

			hits.push_back({ proj->GetOwnerId(), c->GetObjId(), proj->GetDamage() });
			proj->MarkExpired();	// SweepExpiredProjectiles 가 이 틱 끝에 걷어간다
			break;					// 한 틱에 첫 명중 하나만 (관통 없음)
		}
	}

	for (const Hit& h : hits)
		DealDamage(h.attackerId, h.targetId, h.damage);
}

void Room::BroadcastDebugQuadtree()
{
	if ((_tickCount % 4) != 0)	// 틱 20Hz -> ~5Hz
		return;

	// 구독 세션이 하나도 없으면 조립 비용조차 아낀다.
	auto anySubscriber = [&]() -> bool
	{
		for (auto& item : _objects)
		{
			GameObject* o = item.second.get();
			if (o == nullptr || o->GetObjType() != Protocol::OBJECT_PLAYER)
				continue;
			if (GameSessionRef s = static_cast<Player*>(o)->GetSession())
				if (s->WantsQuadtree())
					return true;
		}
		return false;
	};

	if (anySubscriber() == false)
		return;

	Vector<Bounds> nodes;
	_collisionTree.CollectNodeBounds(OUT nodes, 256);

	Protocol::S_DEBUG_QUADTREE pkt;
	for (const Bounds& b : nodes)
	{
		Protocol::DebugRect* r = pkt.add_nodes();
		r->set_minx(b.minX);
		r->set_miny(b.minY);
		r->set_maxx(b.maxX);
		r->set_maxy(b.maxY);
	}

	int32 alive = 0;
	for (auto& item : _objects)
		if (item.second != nullptr && item.second->IsAlive())
			alive++;

	pkt.set_objectcount(static_cast<uint32>(alive));
	pkt.set_buildmicros(_lastTreeBuildMicros);
	pkt.set_collisionmicros(_lastCollisionMicros);
	pkt.set_servertick(static_cast<uint32>(_tickCount));

	const SendBufferRef buffer = ClientPacketHandler::MakeSendBuffer(pkt);
	for (auto& item : _objects)
	{
		GameObject* o = item.second.get();
		if (o == nullptr || o->GetObjType() != Protocol::OBJECT_PLAYER)
			continue;
		if (GameSessionRef s = static_cast<Player*>(o)->GetSession())
			if (s->WantsQuadtree())
				s->Send(buffer);
	}
}

void Room::SweepExpiredProjectiles()
{
	// Leave 가 _objects 를 건드리므로 대상을 먼저 추려두고 지운다.
	Vector<GameObjectRef> dead;

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		if (object == nullptr || object->GetObjType() != Protocol::OBJECT_PROJECTILE)
			continue;

		Projectile* proj = static_cast<Projectile*>(object);

		// 수명 초과 (MarkExpired 로 벽 히트 시 _expireTick=0 이 되므로 여기서 같이 걸린다) 또는 사거리 초과.
		if (_tickCount >= proj->GetExpireTick() || proj->IsOutOfRange())
		{
			dead.push_back(item.second);
		}
	}

	for (GameObjectRef& object : dead)
		Leave(object);
}

void Room::SweepDeadMonsters()
{
	Vector<GameObjectRef> dead;

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		if (object == nullptr || object->GetObjType() != Protocol::OBJECT_MONSTER)
			continue;

		Monster* monster = static_cast<Monster*>(object);
		const uint64 despawnTick = monster->GetDeathDespawnTick();

		if (despawnTick != 0 && _tickCount >= despawnTick)
			dead.push_back(item.second);
	}

	for (GameObjectRef& object : dead)
		Leave(object);	// S_DESPAWN 브로드캐스트
}

/*---------------
	Debug 출력
----------------*/

std::wstring Room::DescribeLevel()
{
	const NavGrid& grid = _level.GetNavGrid();

	WCHAR buffer[256];
	::swprintf_s(buffer, L"level : %d x %d cells, tileSize %d -> %d x %d tiles",
		_level.GetWidth(), _level.GetHeight(), _level.GetTileSize(),
		grid.GetWidth(), grid.GetHeight());

	std::wstring result = buffer;
	result += L"\n  # = blocked, . = walkable";

	for (int32 ty = 0; ty < grid.GetHeight(); ty++)
	{
		result += L"\n  ";

		for (int32 tx = 0; tx < grid.GetWidth(); tx++)
			result += grid.IsWalkable(tx, ty) ? L'.' : L'#';
	}

	return result;
}

std::wstring Room::DescribePath(const Vector<TilePos>& path, TilePos start, TilePos goal)
{
	const NavGrid& grid = _level.GetNavGrid();

	const int32 width = grid.GetWidth();
	const int32 height = grid.GetHeight();

	// 타일 격자를 그대로 문자 버퍼로 만들고 경로를 덧그린다.
	Vector<WCHAR> canvas;
	canvas.resize(static_cast<size_t>(width) * height, L'.');

	for (int32 ty = 0; ty < height; ty++)
	{
		for (int32 tx = 0; tx < width; tx++)
		{
			if (grid.IsWalkable(tx, ty) == false)
				canvas[static_cast<size_t>(ty) * width + tx] = L'#';
		}
	}

	for (const TilePos& tile : path)
	{
		if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height)
			continue;

		canvas[static_cast<size_t>(tile.y) * width + tile.x] = L'*';
	}

	// 경로 위에 점프 포인트를 덧그린다.
	// J가 *를 덮게 두는 이유 : 경로 위의 점프 포인트가 가장 보고 싶은 것인데
	// *가 이기면 그게 전부 가려진다. 사이를 잇는 *는 그대로 남아 경로 모양도 읽힌다.
	// 경로 위에 점프 포인트를 덧그린다.
	// J가 *를 덮게 두는 이유 : 경로 위의 점프 포인트가 가장 보고 싶은 것인데
	// *가 이기면 그게 전부 가려진다. 사이를 잇는 *는 그대로 남아 경로 모양도 읽힌다.
	for (const TilePos& tile : _pathFinder.GetLastOpenedJumpPoints())
	{
		if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height)
			continue;

		canvas[static_cast<size_t>(tile.y) * width + tile.x] = L'J';
	}

	// 경로가 실제로 지나는 점프 포인트는 J 위에 S로 덮어쓴다.
	// 이러면 어느 점프 포인트가 실제 루트로 채택됐는지 한눈에 갈린다.
	// 목록의 첫 원소가 출발 타일이라 시작 표시도 여기서 같이 칠해진다.
	for (const TilePos& tile : _pathFinder.GetLastPathJumpPoints())
	{
		if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height)
			continue;

		canvas[static_cast<size_t>(tile.y) * width + tile.x] = L'S';
	}

	// 시작과 목표는 그 위에 덮어쓴다.
	if (start.x >= 0 && start.y >= 0 && start.x < width && start.y < height)
		canvas[static_cast<size_t>(start.y) * width + start.x] = L'S';

	if (goal.x >= 0 && goal.y >= 0 && goal.x < width && goal.y < height)
		canvas[static_cast<size_t>(goal.y) * width + goal.x] = L'G';

	std::wstring result = L"S = jump point on the path (first S is the start), G = goal, * = path, J = opened but unused jump point, # = blocked";

	for (int32 ty = 0; ty < height; ty++)
	{
		result += L"\n  ";
		result.append(&canvas[static_cast<size_t>(ty) * width], width);
	}

	return result;
}

/*-------------
	충돌 판정
--------------*/

void Room::RebuildCollisionTree()
{
	Bounds rootBounds = Bounds::Make(0, 0, _level.GetWidth() - 1, _level.GetHeight() - 1);

	// 루트 경계를 개체들의 합집합까지 넓힌다.
	//
	// 반지름이 큰 개체가 맵 가장자리에 있으면 경계가 월드 밖으로 삐져나간다.
	// 그런 개체는 어느 자식에도 안 들어가서 루트에 담기는데, 루트가 자기 개체를
	// 품지 못하면 바깥을 향한 질의가 루트에서 통째로 컬링되어 그 개체를 놓친다.
	// (querytest 가 잡아낸 버그다)
	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		if (object == nullptr || object->IsAlive() == false)
			continue;

		rootBounds.Encapsulate(object->GetBounds());
	}

	_collisionTree.Reset(rootBounds);

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		// 죽은 개체는 판정 대상에서 뺀다.
		if (object == nullptr || object->IsAlive() == false)
			continue;

		_collisionTree.Insert(object);
	}
}

void Room::QueryRange(const Bounds& range, OUT Vector<GameObject*>& out)
{
	_collisionTree.QueryRange(range, OUT out);
}

void Room::QueryCircle(int32 centerX, int32 centerY, int32 radius, OUT Vector<GameObject*>& out)
{
	_collisionTree.QueryCircle(centerX, centerY, radius, OUT out);
}

void Room::QueryCircleBruteForce(int32 centerX, int32 centerY, int32 radius,
								 OUT Vector<GameObject*>& out)
{
	out.clear();

	for (auto& item : _objects)
	{
		GameObject* object = item.second.get();

		if (object == nullptr || object->IsAlive() == false)
			continue;

		if (object->OverlapsCircle(centerX, centerY, radius))
			out.push_back(object);
	}
}

int32 Room::RemoveAllDummies()
{
	// Leave 가 _objects 를 건드리므로 먼저 대상을 추려두고 지운다.
	Vector<GameObjectRef> targets;

	for (auto& item : _objects)
	{
		GameObjectRef& object = item.second;

		if (object->GetObjType() != Protocol::OBJECT_PLAYER)
			continue;

		Player* player = static_cast<Player*>(object.get());

		if (player->IsDummy() == false)
			continue;

		targets.push_back(object);
	}

	for (GameObjectRef& object : targets)
		Leave(object);

	return static_cast<int32>(targets.size());
}

/*------
	AI
-------*/

bool Room::ReloadBehaviorTree(const string& name)
{
	const BehaviorTree* before = _btManager.Find(name);
	const BehaviorTree* after = _btManager.Reload(name);

	if (after == nullptr)
		return false;

	// 재로드는 트리 객체를 새로 만들고 블랙보드 슬롯 구성도 바꿀 수 있다.
	// 붙어 있던 인스턴스를 그대로 두면 없어진 주소를 가리키게 되므로 여기서 다시 묶는다.
	for (auto& item : _behaviors)
	{
		const BehaviorTree* bound = item.second.GetTree();

		if (bound == before || bound == after)
			item.second.Bind(after);
	}

	return true;
}

bool Room::AttachBehavior(uint64 objectId, const string& treeName)
{
	if (Find(objectId) == nullptr)
		return false;

	const BehaviorTree* tree = _btManager.Load(treeName);

	if (tree == nullptr)
		return false;

	_behaviors[objectId].Bind(tree);
	return true;
}

bool Room::DetachBehavior(uint64 objectId)
{
	return _behaviors.erase(objectId) > 0;
}

BtInstance* Room::FindBehavior(uint64 objectId)
{
	auto findIt = _behaviors.find(objectId);

	if (findIt == _behaviors.end())
		return nullptr;

	return &findIt->second;
}

BtStatus Room::TickBehavior(uint64 objectId, float deltaTime)
{
	BtInstance* instance = FindBehavior(objectId);

	if (instance == nullptr)
		return BtStatus::Failure;

	GameObjectRef object = Find(objectId);

	if (object == nullptr)
		return BtStatus::Failure;

	BtContext context;
	context.room = this;
	context.self = object.get();
	context.deltaTime = deltaTime;

	return instance->Tick(context);
}

void Room::TickBehaviors(float deltaTime)
{
	if (_behaviorAutoTick == false || _behaviors.empty())
		return;

	// 룸을 떠난 개체의 인스턴스는 여기서 걷어낸다.
	Vector<uint64> stale;

	for (auto& item : _behaviors)
	{
		GameObjectRef object = Find(item.first);

		if (object == nullptr)
		{
			stale.push_back(item.first);
			continue;
		}

		// 죽었거나 피격 경직 중이면 AI 를 멈춘다 (경직이 풀리면 컴포짓 재개 지점에서 이어감).
		if (object->IsAlive() == false || object->IsStunned(_tickCount))
			continue;

		BtContext context;
		context.room = this;
		context.self = object.get();
		context.deltaTime = deltaTime;

		item.second.Tick(context);
	}

	for (uint64 objectId : stale)
		_behaviors.erase(objectId);
}
