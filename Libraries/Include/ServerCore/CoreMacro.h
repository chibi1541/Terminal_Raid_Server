#pragma once

#define OUT

#define NAMESPACE_BEGIN(name)	namespace name {
#define NAMESPACE_END			}

/*---------------
	  Lock
---------------*/

#define USE_MANY_LOCKS(count)	Lock _locks[count];
#define USE_LOCK				USE_MANY_LOCKS(1)
#define	READ_LOCK_IDX(idx)		ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
#define READ_LOCK				READ_LOCK_IDX(0)
#define	WRITE_LOCK_IDX(idx)		WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK				WRITE_LOCK_IDX(0)

/*---------------
	   Log
---------------*/

// GLogger->Write(level, file, line, format, ...)
// Passes format AND args through __VA_ARGS__ together, so there is no trailing-comma problem.
#define LOG_LEVEL(level, ...)											\
	do {																\
		if (GLogger != nullptr)											\
			GLogger->Write(level, __FILE__, __LINE__, __VA_ARGS__);		\
	} while (0)

#ifdef _DEBUG
	#define LOG_TRACE(...)	LOG_LEVEL(LogLevel::Trace, __VA_ARGS__)
	#define LOG_DEBUG(...)	LOG_LEVEL(LogLevel::Debug, __VA_ARGS__)
#else
	#define LOG_TRACE(...)	((void)0)
	#define LOG_DEBUG(...)	((void)0)
#endif

#define LOG_INFO(...)	LOG_LEVEL(LogLevel::Info,  __VA_ARGS__)
#define LOG_WARN(...)	LOG_LEVEL(LogLevel::Warn,  __VA_ARGS__)
#define LOG_ERROR(...)	LOG_LEVEL(LogLevel::Error, __VA_ARGS__)
#define LOG_FATAL(...)	LOG_LEVEL(LogLevel::Fatal, __VA_ARGS__)

/*---------------
	  Crash
---------------*/

// cause is no longer discarded.
// CrashDump::OnFatal logs it (and flushes) first; the null dereference right after
// then trips UnhandledHandler, which writes the .dmp with real EXCEPTION_POINTERS.
#define CRASH(cause)								\
{													\
	CrashDump::OnFatal(cause, __FILE__, __LINE__);	\
	uint32* crash = nullptr;						\
	__analysis_assume(crash != nullptr);			\
	*crash = 0xDEADBEEF;							\
}

#define ASSERT_CRASH(expr)						\
{												\
	if (!(expr))								\
	{											\
		CRASH("ASSERT_CRASH: " #expr);			\
		__analysis_assume(expr);				\
	}											\
}
