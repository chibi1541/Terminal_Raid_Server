#include "pch.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "Protocol/ClientPacketHandler.h"
#include "Room.h"

void GameSession::OnConnected()
{
	GSessionManager.Add(static_pointer_cast<GameSession>(shared_from_this()));
}

void GameSession::OnDisconnected()
{
	GSessionManager.Remove(static_pointer_cast<GameSession>(shared_from_this()));

	// 룸에 들어가 있었다면 빼준다.
	// Room::Leave가 목록에 없는 개체는 그냥 무시하므로 입장 전 종료도 안전하다.
	if (_player != nullptr && GRoom != nullptr)
	{
		GRoom->DoAsync(&Room::Leave, static_pointer_cast<GameObject>(_player));
		_player = nullptr;
	}
}

void GameSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	PacketSessionRef session = static_pointer_cast<PacketSession>(GetSessionRef());

	ClientPacketHandler::HandlePacket(session, buffer, len);
}

void GameSession::OnSend(int32 len)
{

}
