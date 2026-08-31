#pragma once
#include "Session.h"
#include <string>

/*---------------
	AdminSession

	텔넷/nc 호환 라인 프로토콜.
	PacketSession이 아니라 Session을 직접 상속해 개행 단위로 명령을 조립한다.

	접속 → 배너 → "AUTH <token>" → 임의 명령 → "quit"
	인증 전에는 AUTH 외 모든 입력을 거부하고,
	타임아웃/실패 횟수 초과 시 연결을 끊는다.
----------------*/

class AdminSession : public Session
{
	enum
	{
		MAX_LINE_BYTES		= 4096,
		AUTH_TIMEOUT_MS		= 10'000,
		MAX_AUTH_FAIL		= 3,
	};

public:
	AdminSession();
	virtual ~AdminSession();

public:
	void			SendLine(const std::wstring& text);

	bool			IsAuthenticated() const	{ return _authenticated; }
	uint64			GetConnectTick() const	{ return _connectTick; }
	// 인증 없이 AUTH_TIMEOUT_MS를 넘겼는지 (AdminServer의 스윕 스레드가 사용)
	bool			IsAuthExpired(uint64 nowTick) const;

protected:
	virtual void	OnConnected() override;
	virtual int32	OnRecv(BYTE* buffer, int32 len) override;
	virtual void	OnDisconnected() override;

private:
	void			HandleLine(const std::wstring& line);
	void			HandleAuth(const std::wstring& line);
	std::wstring	DescribePeer();

private:
	bool			_authenticated = false;
	int32			_authFailCount = 0;
	uint64			_connectTick = 0;
	std::string		_pending;	// 개행이 오지 않은 잔여 바이트 (UTF-8)
};
