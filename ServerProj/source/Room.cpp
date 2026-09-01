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
	// TODO : 이동 / 전투 갱신.
	// 지금은 틱 루프만 살려둔다. (jobs 명령의 reserved timers가 0이면 루프가 끊긴 것)
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
	_level.BuildEmpty(120, 30, 3);
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
