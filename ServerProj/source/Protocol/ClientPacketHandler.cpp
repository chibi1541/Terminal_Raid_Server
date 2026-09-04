#include "pch.h"
#include "ClientPacketHandler.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "Game/ObjectIdGenerator.h"
#include "Room.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

namespace
{
	enum
	{
		MAX_NAME_LENGTH = 16,
	};
}

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	LOG_WARN(L"[packet] unknown id=%u size=%u", header->id, header->size);

	return false;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	Protocol::S_LOGIN loginPkt;

	if (gameSession->IsLoggedIn())
	{
		// 이미 로그인한 세션이 또 보냈다.
		loginPkt.set_success(false);
		gameSession->Send(ClientPacketHandler::MakeSendBuffer(loginPkt));

		LOG_WARN(L"[login] duplicated login from %s",
			gameSession->GetAddress().GetIpAddress().c_str());
		return true;
	}

	const string& name = pkt.name();

	if (name.empty() || name.size() > MAX_NAME_LENGTH)
	{
		loginPkt.set_success(false);
		gameSession->Send(ClientPacketHandler::MakeSendBuffer(loginPkt));

		LOG_WARN(L"[login] invalid name (len=%d) from %s",
			static_cast<int32>(name.size()),
			gameSession->GetAddress().GetIpAddress().c_str());
		return true;
	}

	// objectId는 Player 생성자가 ObjectIdGenerator로 발급한다.
	PlayerRef player = MakeShared<Player>();
	player->SetName(name);
	player->SetSession(gameSession);

	gameSession->SetPlayer(player);

	loginPkt.set_success(true);

	Protocol::User* user = loginPkt.mutable_user();
	user->set_id(player->GetObjId());
	user->set_name(player->GetName());

	gameSession->Send(ClientPacketHandler::MakeSendBuffer(loginPkt));

	LOG_INFO(L"[login] objectId=%llu name=%hs from %s",
		player->GetObjId(), player->GetName().c_str(),
		gameSession->GetAddress().GetIpAddress().c_str());

	return true;
}

bool Handle_C_PING(PacketSessionRef& session, Protocol::C_PING& pkt)
{
	Protocol::S_PONG pongPkt;
	session->Send(ClientPacketHandler::MakeSendBuffer(pongPkt));

	return true;
}

bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	PlayerRef player = gameSession->GetPlayer();

	if (player == nullptr || GRoom == nullptr)
	{
		Protocol::S_ENTER_ROOM enterPkt;
		enterPkt.set_success(false);
		gameSession->Send(ClientPacketHandler::MakeSendBuffer(enterPkt));

		LOG_WARN(L"[room] enter before login from %s",
			gameSession->GetAddress().GetIpAddress().c_str());
		return true;
	}

	// 중복 입장은 Room::Enter가 걸러낸다.
	// 여기서 player->room을 보면 룸 스레드가 쓰는 값을 읽게 되므로 확인하지 않는다.
	GRoom->DoAsync(&Room::Enter, static_pointer_cast<GameObject>(player), true);

	return true;
}

bool Handle_C_MOVE(PacketSessionRef& session, Protocol::C_MOVE& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	PlayerRef player = gameSession->GetPlayer();

	if (player == nullptr || GRoom == nullptr)
		return true;

	// 룸 상태를 IOCP 워커에서 만지지 않는다. 룸 잡 큐로 넘긴다.
	GRoom->DoAsync(&Room::HandleMove, static_pointer_cast<GameObject>(player),
		pkt.inputseq(), pkt.clienttick(), static_cast<int32>(pkt.dir()));

	return true;
}

bool Handle_C_EXIT_ROOM(PacketSessionRef& session, Protocol::C_EXIT_ROOM& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	PlayerRef player = gameSession->GetPlayer();

	if (player != nullptr && GRoom != nullptr)
		GRoom->DoAsync(&Room::Leave, static_pointer_cast<GameObject>(player));

	Protocol::S_EXIT_ROOM exitPkt;
	gameSession->Send(ClientPacketHandler::MakeSendBuffer(exitPkt));

	return true;
}
