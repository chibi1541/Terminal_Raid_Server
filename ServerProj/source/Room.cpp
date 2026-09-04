#include "pch.h"
#include "Room.h"
#include "Protocol/ClientPacketHandler.h"
#include "Game/Player.h"
#include "Game/ObjectIdGenerator.h"
#include "GameSession.h"
#include <chrono>

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

	DoTimer(TICK_INTERVAL_MS, &Room::Tick);

	LOG_INFO(L"[room] begin play (%u x %u)", GetWidth(), GetHeight());
}

void Room::Tick()
{
	// 이번 틱에 쓸 공간 인덱스를 먼저 세운다.
	// 이 아래에서 도는 이동 / 전투 로직은 전부 이 트리를 보게 된다.
	RebuildCollisionTree();

	// 붙어 있는 AI 를 돌린다. 공간 인덱스를 세운 뒤라 리프가 질의를 쓸 수 있다.
	TickBehaviors(GetTickDeltaTime());

	// TODO : 이동 / 전투 갱신.
	// (jobs 명령의 reserved timers가 0이면 틱 루프가 끊긴 것)
	DoTimer(TICK_INTERVAL_MS, &Room::Tick);
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
		object->SetPos(FindSpawnPos());

	object->SetRoom(static_pointer_cast<Room>(shared_from_this()));

	// 목록에 넣기 '전에' 알린다.
	// 이렇게 하면 S_SPAWN이 본인에게는 가지 않고,
	// 본인은 바로 아래 S_ENTER_ROOM으로 전체 목록을 한 번에 받는다.
	{
		Protocol::S_SPAWN spawnPkt;
		object->FillObjectInfo(spawnPkt.add_objects());
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

		::swprintf_s(buffer, L"\n  id=%llu type=%d pos=(%d, %d) hp=%d/%d %hs%hs",
			object->GetObjId(), static_cast<int32>(object->GetObjType()),
			object->GetPosX(), object->GetPosY(), object->GetHp(), object->GetMaxHp(),
			(player != nullptr) ? player->GetName().c_str() : "",
			(player != nullptr && player->IsDummy()) ? " [dummy]" : "");

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
	static const WCHAR* LEVEL_PATH = L"../Config/Level01.xml";

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

		BtContext context;
		context.room = this;
		context.self = object.get();
		context.deltaTime = deltaTime;

		item.second.Tick(context);
	}

	for (uint64 objectId : stale)
		_behaviors.erase(objectId);
}
