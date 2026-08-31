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
}

void GameSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	PacketSessionRef session = static_pointer_cast<PacketSession>(GetSessionRef());
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

	ClientPacketHandler::HandlePacket(session, buffer, len);
}

void GameSession::OnSend(int32 len)
{

}
