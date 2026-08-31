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
#include "Debug/GameCommands.h"
#include "AdminServer.h"
#include "CommandRegistry.h"
#include "ConsoleCommand.h"
#include "ServerStats.h"
#include "CrashDump.h"

enum
{
	WORKER_TICK			= 64,
	WORKER_COUNT		= 8,
	MAX_SESSION_COUNT	= 100,
	GAME_PORT			= 7777,
	ADMIN_PORT			= 7778,
};

// quit 명령 / Ctrl+C 가 내리는 종료 플래그.
Atomic<bool> GRunning = true;

void DoWokerJob(ServerServiceRef& service)
{
	while (GRunning.load())
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

// Admin 서버 토큰은 소스에 박지 않는다. 환경변수가 없으면 Admin 서버를 아예 띄우지 않는다.
static std::wstring ReadAdminToken()
{
	WCHAR* value = nullptr;
	size_t length = 0;

	if (::_wdupenv_s(&value, &length, L"TR_ADMIN_TOKEN") != 0 || value == nullptr)
		return std::wstring();

	std::wstring token = value;
	::free(value);

	return token;
}

int main()
{
	CrashDump::Init();
	SetRandomSeed32();

	ClientPacketHandler::Init();

	// Room은 StlAllocator 컨테이너를 들고 있어서 GMemory가 준비된 뒤에 만들어야 한다.
	// (자세한 내용은 Room.h 주석)
	GRoom = MakeShared<Room>();
	GRoom->BeginPlay();

	// 게임 상태를 만지는 명령이 룸의 JobQueue를 타고 들어가게 한다.
	GCommandRegistry->SetGameJobQueue(GRoom);
	GCommandRegistry->SetShutdownHandler([]() { GRunning.store(false); });
	GameCommands::Register();

	ServerServiceRef service = MakeShared<ServerService>(
		NetAddress(L"127.0.0.1", GAME_PORT),
		MakeShared<IocpCore>(),
		MakeShared<GameSession>,
		MAX_SESSION_COUNT);

	ASSERT_CRASH(service->Start());

	GServerStats->RegisterService(service);
	GServerStats->SetWorkerThreadCount(WORKER_COUNT + 1);

	AdminServer adminServer;
	const std::wstring adminToken = ReadAdminToken();

	if (adminToken.empty())
	{
		LOG_WARN(L"[admin] TR_ADMIN_TOKEN is not set, remote admin server is disabled");
	}
	else if (adminServer.Start(NetAddress(L"127.0.0.1", ADMIN_PORT), adminToken) == false)
	{
		LOG_ERROR(L"[admin] failed to start on port %d", static_cast<int32>(ADMIN_PORT));
	}

	// stdin 명령 루프. 'help'로 목록을 볼 수 있다.
	GConsoleCommand->Start();

	LOG_INFO(L"[server] listening on 127.0.0.1:%d", static_cast<int32>(GAME_PORT));

	for (int32 i = 0; i < WORKER_COUNT; i++)
	{
		GThreadManager->Launch([&service]()
			{
				DoWokerJob(service);
			});
	}

	DoWokerJob(service);

	GThreadManager->Join();

	/*----------------
		종료 처리
	-----------------*/

	GConsoleCommand->Stop();
	adminServer.Stop();
	service->CloseService();

	// GRoom은 반드시 여기서 놓는다.
	// 정적 소멸 시점까지 살려두면
	// ~JobQueue -> LockQueue::Clear -> WRITE_LOCK -> DeadLockProfiler 경로가
	// 이미 파괴된 thread_local을 건드려 크래시난다. (CommandRegistry.h 주석 참고)
	GJobTimer->Clear();
	GRoom->ClearJobs();
	GRoom = nullptr;

	LOG_INFO(L"[server] shutdown");
}
