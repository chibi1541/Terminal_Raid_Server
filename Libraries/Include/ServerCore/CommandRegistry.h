#pragma once
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <mutex>

/*---------------
	CommandRegistry

	콘솔(stdin)과 원격 Admin 세션이 공유하는 단일 명령 테이블.
	응답은 CommandReply 콜백으로 나가므로 입력 경로가 무엇이든 핸들러는 동일하다.

	실행 모델
	- Immediate  : 호출한 스레드에서 바로 실행. 원자 카운터만 읽는 조회형 명령용.
	- GameThread : SetGameJobQueue()로 받은 JobQueue에 DoAsync로 밀어넣는다.
	               게임 상태를 건드리는 명령은 이 모드로 등록해 기존 잡 큐의
	               직렬화 보장을 그대로 탄다.
----------------*/

using CommandReply = std::function<void(const std::wstring&)>;

struct CommandContext
{
	std::vector<std::wstring>	args;		// args[0] == 명령 이름
	CommandReply				reply;
	bool						isRemote = false;

	void				Reply(const WCHAR* format, ...);
	const std::wstring&	Arg(size_t index) const;
	size_t				ArgCount() const { return args.size(); }
	bool				HasFlag(const WCHAR* flag) const;
};

enum class CommandRunMode : uint8
{
	Immediate,
	GameThread,
};

using CommandHandler = std::function<void(CommandContext&)>;

class CommandRegistry
{
public:
	struct Entry
	{
		std::wstring	name;
		std::wstring	usage;
		std::wstring	description;
		CommandHandler	handler;
		CommandRunMode	mode = CommandRunMode::Immediate;
	};

public:
	void			Register(const WCHAR* name, const WCHAR* usage, const WCHAR* description,
							 CommandHandler handler, CommandRunMode mode = CommandRunMode::Immediate);
	bool			Unregister(const WCHAR* name);

	// 알 수 없는 명령이면 false. 파싱/디스패치 실패도 reply로 알린다.
	bool			Execute(const std::wstring& line, CommandReply reply, bool isRemote);

	void			SetGameJobQueue(JobQueueRef jobQueue);
	void			SetShutdownHandler(std::function<void()> handler);

	void			RegisterBuiltins();
	std::wstring	BuildHelp(const WCHAR* name = nullptr);

public:
	static std::vector<std::wstring>	Tokenize(const std::wstring& line);
	static std::wstring					ToLower(const std::wstring& text);

private:
	mutable std::mutex					_mutex;
	std::map<std::wstring, Entry>		_commands;	// key : 소문자 명령 이름
	std::vector<std::wstring>			_order;		// 등록 순서 (help 출력용)
	// 약한 참조로 든다.
	// 전역이 정적 소멸 시점에 JobQueue를 풀어버리면
	// ~JobQueue -> LockQueue::Clear -> WRITE_LOCK -> DeadLockProfiler 경로가
	// 이미 파괴된 thread_local(LLockStack)을 건드려 크래시난다.
	// 집관은 소비 프로젝트에 남겨둔다.
	std::weak_ptr<JobQueue>				_gameJobQueue;
	std::function<void()>				_shutdownHandler;
	bool								_builtinsRegistered = false;
};
