#include "pch.h"
#include "Room.h"
#include "Protocol/ClientPacketHandler.h"
#include "Game/Player.h"
#include "Game/ObjectIdGenerator.h"
#include "GameSession.h"

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
	DoTimer(TICK_INTERVAL_MS, &Room::Tick);

	LOG_INFO(L"[room] begin play (%u x %u)", _width, _height);
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
	enterPkt.set_width(_width);
	enterPkt.set_height(_height);
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
	Protocol::Vector2 pos;
	pos.set_x(RandomRange32(1, static_cast<int32>(_width) - 2));
	pos.set_y(RandomRange32(1, static_cast<int32>(_height) - 2));

	// TODO : 다른 개체와 겹치는지 확인. 지금은 겹쳐도 그냥 둔다.
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
		_width, _height, static_cast<int32>(_objects.size()));

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
