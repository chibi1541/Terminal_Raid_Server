#pragma once
#include "Game/GameObject.h"
#include "Protocol/Enum.pb.h"

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

	Protocol::CharacterType	GetCharacterType() const	{ return _characterType; }

	// CharacterData 테이블을 조회해 충돌 박스 / 반경 / 체력 / 공격력을 정한다.
	// ★ 이동 속도는 건드리지 않는다 - 클라 예측(MoveMath 상수)과 분리돼 있다.
	void					SetCharacterType(Protocol::CharacterType type);

	// 마지막 발사 시각(GetTickCount64). 0 = 아직 안 쏨. Room::HandleAttack 이 쿨다운에 쓴다.
	uint64	GetLastAttackWallMs() const			{ return _lastAttackWallMs; }
	void	SetLastAttackWallMs(uint64 ms)		{ _lastAttackWallMs = ms; }

private:
	// TODO : Set UserID
	uint64					_userId = 0;
	string					_name;
	weak_ptr<GameSession>	_session;
	uint64					_lastAttackWallMs = 0;
	Protocol::CharacterType	_characterType = Protocol::CHARACTER_NONE;
};

using PlayerRef = shared_ptr<Player>;
