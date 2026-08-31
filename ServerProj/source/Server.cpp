#include "pch.h"
#include "ThreadManager.h"
#include "SocketUtils.h"
#include "Listener.h"
#include "Service.h"
#include "Session.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "Protocol/ClientPacketHandler.h"
#include "Protocol/Protocol.pb.h"
#include "Room.h"
#include "DBConnectionPool.h"
#include "DBBind.h"
#include "XmlParser.h"
#include "DBSynchronizer.h"

enum
{
	WORKER_TICK = 64
};

void DoWokerJob(ServerServiceRef& service)
{
	// TODO : 종료 판정
	while (true)
	{
		// 이거 dispatch 밑으로 내려도 될려나?
		// 아 내리면 일감 몰렸을 때 쓰레드가 노예 상태가 되는 건 못 막는 구나 
		LEndTickCount = ::GetTickCount64() + WORKER_TICK;

		// 네트워크 입출력 처리 -> 인게임 로직까지 (패킷 핸들러에 의해)
		service->GetIocpCore()->Dispatch(10);

		// 글로벌 큐
		ThreadManager::DoGlobalQueueWork();

		ThreadManager::DistributeReservedJobs();
	}
}


int main()
{
	ClientPacketHandler::Init();

	ServerServiceRef service = MakeShared<ServerService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		MakeShared<GameSession>,
		100);

	ASSERT_CRASH(service->Start());

	for (int32 i = 0; i < 8; i++)
	{
		GThreadManager->Launch([&service]()
			{
				DoWokerJob(service);
			});
	}

	DoWokerJob(service);

	GThreadManager->Join();
}