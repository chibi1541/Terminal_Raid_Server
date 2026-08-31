#pragma once
#include "Game/GameObject.h"

class GameSession;

/*-----------
	Player

	유저가 조종하는 개체.
	위치나 체력처럼 몬스터도 갖는 값은 전부 GameObject에 있다.
------------*/

class Player : public GameObject
{
public:
	Player();

	virtual void FillObjectInfo(Protocol::ObjectInfo* info) override;

	// 디버그 명령(spawn)으로 만든 더미 플레이어는 세션이 없다.
	// 이미 끊긴 세션도 여기서 같이 걸린다.
	bool IsDummy() const { return _session.expired(); }

public:
	uint64					GetUserId() const	{ return _userId; }
	const string&			GetName() const		{ return _name; }
	// 잠근 결과를 돌려준다. 끊긴 세션이면 nullptr.
	shared_ptr<GameSession>	GetSession() const	{ return _session.lock(); }

	void SetUserId(uint64 userId)						{ _userId = userId; }
	void SetName(const string& name)					{ _name = name; }
	void SetSession(shared_ptr<GameSession> session)	{ _session = session; }

private:
	// TODO : Set UserID
	uint64					_userId = 0;
	string					_name;
	weak_ptr<GameSession>	_session;
};

using PlayerRef = shared_ptr<Player>;
