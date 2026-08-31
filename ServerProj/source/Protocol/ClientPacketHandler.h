#pragma once
#include "Protocol.pb.h"

// jinja2 템플릿 엔진을 활용한 코드 자동화용 템플릿

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];

// PKT enum 자동화
enum : uint16
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_PING = 1002,
	PKT_S_PONG = 1003,
	PKT_C_ENTER_ROOM = 1004,
	PKT_S_ENTER_ROOM = 1005,
	PKT_C_EXIT_ROOM = 1006,
	PKT_S_EXIT_ROOM = 1007,
};

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);

// PKT handle 함수 자동 선언, 선언부만 만들어주기 때문에 정의부를 따로 생성해야 함
bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt);
bool Handle_C_PING(PacketSessionRef& session, Protocol::C_PING& pkt);
bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt);
bool Handle_C_EXIT_ROOM(PacketSessionRef& session, Protocol::C_EXIT_ROOM& pkt);

// PacketHandler 클래스 자동화
class ClientPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; ++i)
			GPacketHandler[i] = Handle_INVALID;

		// Handler 함수 등록 자동화
		GPacketHandler[PKT_C_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_LOGIN>(Handle_C_LOGIN, session, buffer, len); };
		GPacketHandler[PKT_C_PING] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_PING>(Handle_C_PING, session, buffer, len); };
		GPacketHandler[PKT_C_ENTER_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_ENTER_ROOM>(Handle_C_ENTER_ROOM, session, buffer, len); };
		GPacketHandler[PKT_C_EXIT_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_EXIT_ROOM>(Handle_C_EXIT_ROOM, session, buffer, len); };

	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}

	// sendbuffer 작성 자동화, 선언부만 만들어주기 때문에 정의부를 따로 생성해야 함
	static SendBufferRef MakeSendBuffer(Protocol::S_LOGIN& pkt) {return MakeSendBuffer(pkt, PKT_S_LOGIN); }
	static SendBufferRef MakeSendBuffer(Protocol::S_PONG& pkt) {return MakeSendBuffer(pkt, PKT_S_PONG); }
	static SendBufferRef MakeSendBuffer(Protocol::S_ENTER_ROOM& pkt) {return MakeSendBuffer(pkt, PKT_S_ENTER_ROOM); }
	static SendBufferRef MakeSendBuffer(Protocol::S_EXIT_ROOM& pkt) {return MakeSendBuffer(pkt, PKT_S_EXIT_ROOM); }


private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, uint32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(session, pkt);
	}

	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		uint16 packetSize = dataSize + sizeof(PacketHeader);

		SendBufferRef sendBuffer = GSendBufferManager->Open(packetSize);
		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;
		ASSERT_CRASH(pkt.SerializeToArray((&header[1]), dataSize));
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};
