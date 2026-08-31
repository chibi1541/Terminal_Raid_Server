#pragma once

/*---------------
	CrashDump

	미처리 예외 / CRASH 매크로 발생 시 .dmp 미니덤프와
	직전 로그 링버퍼(.log)를 같은 이름으로 짝지어 남긴다.

	라이브러리가 자동으로 켜지 않는다. 소비 프로젝트의 main()에서
	CrashDump::Init()을 명시적으로 호출해야 한다.
----------------*/

class CrashDump
{
public:
	// dumpTypeFlags == 0 이면 기본 조합 사용
	static bool			Init(const WCHAR* dumpDir = L"Dumps", uint32 dumpTypeFlags = 0);
	static bool			IsInitialized();

	// CRASH 매크로에서 호출된다. 로그만 남기고 돌아온다.
	// 실제 .dmp는 뒤이은 널 역참조가 UnhandledHandler를 태우면서 기록된다.
	static void			OnFatal(const char* cause, const char* file, int32 line);

	// 예외 없이 현재 시점 스냅샷 덤프를 남긴다 (프로세스는 계속 살아있다)
	static bool			WriteDumpNow(const WCHAR* reason);

	static LONG WINAPI	UnhandledHandler(EXCEPTION_POINTERS* exceptionPointers);

	static const WCHAR*	GetDumpDir();
	static const WCHAR*	GetLastDumpPath();

private:
	static bool			WriteDump(EXCEPTION_POINTERS* exceptionPointers, const WCHAR* reason, bool oneShot);
	static void			InstallHandlers();
	static void			MakeDumpBaseName(OUT WCHAR* buffer, int32 count);
};
