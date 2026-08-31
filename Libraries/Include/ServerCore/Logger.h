#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <string>
#include <vector>

/*---------------
	Logger

	- 전용 스레드에서 소비하는 비동기 로거
	- 콘솔 싱크는 기존 ConsoleLog(GConsoleLogger)를 그대로 재사용한다
	- 파일 싱크는 UTF-8, 일자 롤링
	- 최근 RING_CAPACITY개를 링 버퍼로 들고 있다가 크래시/logdump 시 토해낸다

	주의 : 내부 컨테이너는 의도적으로 std의 기본 allocator를 쓴다.
	       Memory/MemoryPool이 ASSERT_CRASH를 때리는 상황에서도 로그를 남겨야 하므로
	       GMemory(StlAllocator)에 재진입하면 안 된다.
----------------*/

enum class LogLevel : uint8
{
	Trace,
	Debug,
	Info,
	Warn,
	Error,
	Fatal,
	Off,
};

class Logger
{
public:
	enum
	{
		RING_CAPACITY	= 4096,
		MAX_MESSAGE_LEN	= 2048,
		MAX_LINE_LEN	= 2560,
	};

	struct Entry
	{
		LogLevel		level = LogLevel::Info;
		uint32			threadId = 0;
		SYSTEMTIME		time = {};
		const char*		file = nullptr;	// __FILE__ (정적 문자열이므로 복사하지 않는다)
		int32			line = 0;
		std::wstring	message;
		bool			raw = false;	// true면 헤더 없이 본문만 출력 (명령 응답)
	};

public:
	Logger();
	~Logger();

	// 파일 싱크 활성화. 호출하지 않으면 콘솔로만 나간다.
	bool			Init(const WCHAR* dir = L"Logs", const WCHAR* prefix = L"Server");
	void			Shutdown();

	void			Write(LogLevel level, const char* file, int32 line, const WCHAR* format, ...);
	void			WriteRaw(const WCHAR* format, ...);			// 레벨 필터/헤더 없이 콘솔+파일
	void			WriteFatalDirect(const WCHAR* text);		// 크래시 경로 : 큐/힙 우회, 동기 기록

	void			Flush(uint32 timeoutMs = 3000);

	void			SetFileLevel(LogLevel level)	{ _fileLevel.store(level); }
	void			SetConsoleLevel(LogLevel level)	{ _consoleLevel.store(level); }
	LogLevel		GetFileLevel() const			{ return _fileLevel.load(); }
	LogLevel		GetConsoleLevel() const			{ return _consoleLevel.load(); }
	bool			IsEnabled(LogLevel level) const;

	bool			DumpRingBuffer(const WCHAR* path);
	std::wstring	GetLogFilePath() const;

public:
	static const WCHAR*	LevelToString(LogLevel level);
	static bool			ParseLevel(const WCHAR* text, OUT LogLevel& outLevel);
	static bool			MakeDirectories(const WCHAR* path);
	static std::string	ToUtf8(const std::wstring& text);

private:
	void			Enqueue(Entry&& entry);
	void			WorkerLoop();
	void			Consume(Entry& entry);
	std::wstring	FormatLine(const Entry& entry) const;
	void			RollFileIfNeeded(const SYSTEMTIME& now);
	void			WriteToFile(const std::wstring& line);
	void			WriteToConsole(LogLevel level, const std::wstring& line);
	void			PushRing(const std::wstring& line);
	static const char* BaseName(const char* path);

private:
	Atomic<LogLevel>		_fileLevel{ LogLevel::Info };
	Atomic<LogLevel>		_consoleLevel{ LogLevel::Info };

	std::mutex				_queueMutex;
	std::condition_variable	_queueCv;
	std::condition_variable	_drainCv;
	std::deque<Entry>		_queue;
	uint64					_pushedCount = 0;
	uint64					_consumedCount = 0;
	bool					_stopRequested = false;

	std::thread				_worker;
	Atomic<bool>			_running{ false };
	Atomic<bool>			_shutdownDone{ false };

	// 워커 스레드 전용
	FILE*					_file = nullptr;
	std::wstring			_dir;
	std::wstring			_prefix;
	std::wstring			_filePath;
	int32					_fileDate = 0;	// yyyymmdd
	bool					_fileEnabled = false;

	// 링 버퍼 (DumpRingBuffer가 다른 스레드에서 호출되므로 별도 뮤텍스)
	mutable std::mutex		_ringMutex;
	std::vector<std::wstring> _ring;
	uint32					_ringHead = 0;
	uint32					_ringCount = 0;

	// 크래시 경로용 : Init 시점에 미리 만들어 둔다 (핸들러 안에서 할당 금지)
	WCHAR					_fatalFilePath[MAX_PATH] = {};
};
