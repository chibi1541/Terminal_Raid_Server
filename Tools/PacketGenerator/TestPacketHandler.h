#pragma once
#include "Protocol.pb.h"

// jinja2 템플릿 엔진을 활용한 코드 자동화용 템플릿

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];

// PKT enum 자동화
enum : uint16
{
	PKT_S_TEST = 1000,
};

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);

// PKT handle 함수 자동 선언, 선언부만 만들어주기 때문에 정의부를 따로 생성해야 함

// PacketHandler 클래스 자동화
class TestPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; ++i)
			GPacketHandler[i] = Handle_INVALID;

		// Handler 함수 등록 자동화

	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}

	// sendbuffer 작성 자동화


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
