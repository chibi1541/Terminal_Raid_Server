#pragma once
#include "Game/GameObject.h"
#include "Protocol/Enum.pb.h"

/*-----------
	Monster

	위치나 체력처럼 플레이어도 갖는 값은 전부 GameObject에 있다.
------------*/

class Monster : public GameObject
{
public:
	Monster();

	virtual void FillObjectInfo(Protocol::ObjectInfo* info) override;

public:
	Protocol::MonsterType	GetMonsterType() const	{ return _monsterType; }
	const string&			GetMonsterTypeName() const	{ return _monsterTypeName; }	// 디버그 표시용

	// MonsterData 테이블을 조회해 풋프린트 / 충돌 박스 / 반경 / 체력 / 공격력 / 이동속도를 정한다.
	void					SetMonsterType(Protocol::MonsterType type);

private:
	Protocol::MonsterType	_monsterType = Protocol::Monster_None;
	string					_monsterTypeName;	// MonsterType_Name(type) 캐시 (디버그 표시)
};

using MonsterRef = shared_ptr<Monster>;
