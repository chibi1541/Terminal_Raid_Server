#pragma once
#include "Game/Player.h"

class GameSession : public PacketSession
{

public:
	~GameSession()
	{
		cout << "~GameSession" << endl;
	}

	virtual void OnConnected() override;
	virtual void OnDisconnected() override;
	virtual void OnRecvPacket(BYTE* buffer, int32 len) override;
	virtual void OnSend(int32 len) override;

public:
	// C_LOGIN 처리에서 생성된다. 로그인 전에는 nullptr.
	// 이 세션을 처리하는 IOCP 워커에서만 건드린다.
	PlayerRef	GetPlayer() const			{ return _player; }
	void		SetPlayer(PlayerRef player)	{ _player = player; }
	bool		IsLoggedIn() const			{ return _player != nullptr; }

private:
	PlayerRef _player;
};

using GameSessionRef = shared_ptr<GameSession>;
