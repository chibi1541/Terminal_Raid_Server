#pragma once
#include "pch.h"
#include "CorePch.h"

class Lock
{
	enum : uint32
	{
		AQUIRE_TIMEOUT_TICK = 10'000,
		MAX_SPIN_COUNT = 5'000,
		// 32비트 중 상위 16비트에 해당하는 값
		// Write Lock을 점유 중인 쓰레드 ID를 체크하기 위한 마스크
		WRITE_THREAD_MASK = 0xFFFF'0000,
		// 32비트 중 하위 16비트에 해당하는 값
		// Read Lock을 점유 중인 쓰레드의 수를 체크하기 위한 마스크
		READ_COUNT_MASK = 0x0000'FFFF,
		EMPTY_FLAG = 0x0000'0000
	};

public:
	void WriteLock(const char* name);
	void WriteUnlock(const char* name);
	void ReadLock(const char* name);
	void ReadUnlock(const char* name);

private:
	Atomic<uint32> _lockFlag = EMPTY_FLAG;
	uint32 _writeCount = 0;
};

class ReadLockGuard
{	
public:
	ReadLockGuard(Lock& lock, const char* name) : _lock(lock), _name(name) { _lock.ReadLock(name); }
	~ReadLockGuard() {_lock.ReadUnlock(_name);}

private:
	Lock& _lock;
	const char* _name;
};

class WriteLockGuard
{
public:
	WriteLockGuard(Lock& lock, const char* name) : _lock(lock), _name(name) { _lock.WriteLock(name); }
	~WriteLockGuard() {_lock.WriteUnlock(_name);}

private:	
	Lock& _lock;
	const char* _name;
};