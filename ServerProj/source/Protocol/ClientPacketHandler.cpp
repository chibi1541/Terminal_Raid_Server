#include "pch.h"
#include "ClientPacketHandler.h"
#include "GameSession.h"
#include "GameSessionManager.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	Protocol::S_LOGIN loginPkt;
	loginPkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(loginPkt);
	session->Send(sendBuffer);

	LOG_INFO(L"Send S_LOGIN %s", session->GetAddress().GetIpAddress().c_str());

	return true;
}

bool Handle_C_PING(PacketSessionRef& session, Protocol::C_PING& pkt)
{
	return true;
}

bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt)
{
	return true;
}

bool Handle_C_EXIT_ROOM(PacketSessionRef& session, Protocol::C_EXIT_ROOM& pkt)
{
	return true;
}
