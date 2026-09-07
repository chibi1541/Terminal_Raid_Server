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

	// 디버그 오버레이 구독 상태. C_DEBUG_CONFIG로 갱신된다.
	// IOCP 워커에서 쓰고 룸 스레드에서 읽는다 - bool 원자성에 기댄다(디버그 전용, 정확할 필요 없음).
	bool		WantsLevelGrid() const		{ return _wantLevelGrid; }
	bool		WantsPaths() const			{ return _wantPaths; }
	bool		WantsQuadtree() const		{ return _wantQuadtree; }
	void		SetDebugConfig(bool wantLevelGrid, bool wantPaths, bool wantQuadtree)
	{
		_wantLevelGrid = wantLevelGrid;
		_wantPaths = wantPaths;
		_wantQuadtree = wantQuadtree;
	}

private:
	PlayerRef _player;
	bool _wantLevelGrid = false;
	bool _wantPaths = false;
	bool _wantQuadtree = false;
};

using GameSessionRef = shared_ptr<GameSession>;
