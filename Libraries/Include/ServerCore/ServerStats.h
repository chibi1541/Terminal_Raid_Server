#pragma once
#include <mutex>
#include <vector>
#include <string>

/*---------------
	ServerStats

	런타임 카운터 수집기. 대시보드(status / sessions / mem / jobs)의 데이터 소스.
	계측 지점은 Session::ProcessRecv/ProcessSend, Service::AddSession/ReleaseSession,
	JobQueue::Execute 세 곳뿐이므로 핫패스 비용은 relaxed atomic 증가 1회다.
----------------*/

class Service;

struct StatSnapshot
{
	uint64	captureTick = 0;
	uint64	uptimeMs = 0;

	int32	sessionCount = 0;
	int32	maxSessionCount = 0;
	int32	serviceCount = 0;

	uint64	acceptCount = 0;
	uint64	disconnectCount = 0;

	uint64	recvBytes = 0;
	uint64	sendBytes = 0;
	uint64	recvCount = 0;
	uint64	sendCount = 0;

	uint64	jobExecuted = 0;
	int32	globalQueueSize = 0;
	int32	reservedTimerCount = 0;

	int32	sendBufferChunkCount = 0;

	int32	memPoolUseCount = 0;
	int32	memPoolReserveCount = 0;
	bool	memPoolMeaningful = false;	// _STOMP가 켜져 있으면 false

	uint32	workerThreadCount = 0;
};

class ServerStats
{
public:
	ServerStats();

public:
	/* 계측 (핫패스) */
	void			OnAccept()					{ _acceptCount.fetch_add(1, std::memory_order_relaxed); }
	void			OnDisconnect()				{ _disconnectCount.fetch_add(1, std::memory_order_relaxed); }
	void			OnRecv(int32 bytes)			{ _recvBytes.fetch_add(static_cast<uint64>(bytes), std::memory_order_relaxed); _recvCount.fetch_add(1, std::memory_order_relaxed); }
	void			OnSend(int32 bytes)			{ _sendBytes.fetch_add(static_cast<uint64>(bytes), std::memory_order_relaxed); _sendCount.fetch_add(1, std::memory_order_relaxed); }
	void			OnJobExecuted(int32 count)	{ _jobExecuted.fetch_add(static_cast<uint64>(count), std::memory_order_relaxed); }

public:
	/* 조회 */
	void			RegisterService(std::shared_ptr<Service> service);
	void			SetWorkerThreadCount(uint32 count)	{ _workerThreadCount.store(count); }
	uint64			GetStartTick() const				{ return _startTick; }

	StatSnapshot	Capture();
	std::wstring	Format(const StatSnapshot& current);
	std::wstring	FormatWithDelta(const StatSnapshot& prev, const StatSnapshot& current);
	std::wstring	FormatSessions(bool verbose);
	bool			DumpToFile(const WCHAR* path);

private:
	uint64			_startTick = 0;

	// 핫 카운터는 캐시라인을 나눠 false sharing을 피한다
	alignas(64) Atomic<uint64>	_recvBytes{ 0 };
	alignas(64) Atomic<uint64>	_sendBytes{ 0 };
	alignas(64) Atomic<uint64>	_recvCount{ 0 };
	alignas(64) Atomic<uint64>	_sendCount{ 0 };
	alignas(64) Atomic<uint64>	_jobExecuted{ 0 };

	Atomic<uint64>	_acceptCount{ 0 };
	Atomic<uint64>	_disconnectCount{ 0 };
	Atomic<uint32>	_workerThreadCount{ 0 };

	mutable std::mutex						_serviceMutex;
	std::vector<std::weak_ptr<Service>>		_services;
};
