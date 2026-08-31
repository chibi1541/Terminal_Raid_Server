#pragma once
#include "NetAddress.h"
#include <thread>
#include <string>

/*---------------
	AdminServer

	원격 대시보드 접속점.

	게임 워커가 데드락/스톨이어도 진단이 가능해야 하므로
	자체 IocpCore와 자체 워커 스레드를 갖는다. (ThreadManager를 쓰지 않는다)

	기본 바인드는 127.0.0.1이며 토큰이 없으면 Start()가 실패한다.
----------------*/

class AdminServer
{
public:
	~AdminServer();

	// address 기본값은 127.0.0.1. token은 비어 있으면 실패한다.
	bool				Start(NetAddress address, const std::wstring& token, int32 maxSessionCount = 4);
	void				Stop();
	bool				IsRunning() const { return _running.load(); }

public:
	/* AdminSession이 사용하는 설정 */
	static bool			VerifyToken(const std::wstring& candidate);
	static bool			HasToken();

private:
	void				WorkerLoop();

private:
	ServerServiceRef	_service;
	IocpCoreRef			_core;
	std::thread			_worker;
	Atomic<bool>		_running{ false };
};
