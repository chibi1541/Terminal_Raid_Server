#pragma once
#include "Game/GameObject.h"

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
	const string&	GetMonsterTypeName() const		{ return _monsterTypeName; }
	void			SetMonsterTypeName(const string& name)	{ _monsterTypeName = name; }

private:
	string	_monsterTypeName;	// 디버그 표시 / 향후 몬스터 종류 식별용
};

using MonsterRef = shared_ptr<Monster>;
