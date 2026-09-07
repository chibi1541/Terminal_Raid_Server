#include "pch.h"
#include "Game/Monster.h"
#include "Game/MonsterData.h"
#include "Game/ObjectIdGenerator.h"

Monster::Monster()
{
	SetObjType(Protocol::OBJECT_MONSTER);

	// objectId 상위 16비트가 타입이므로 타입을 정한 직후에 발급한다.
	SetObjId(ObjectIdGenerator::GenerateObjectId(GetObjType()));
}

void Monster::FillObjectInfo(Protocol::ObjectInfo* info)
{
	GameObject::FillObjectInfo(info);

	// 클라가 이 타입으로 MonsterData 를 조회해 스프라이트/반경을 정한다.
	info->mutable_monster()->set_monstertype(_monsterType);
}

void Monster::SetMonsterType(Protocol::MonsterType type)
{
	_monsterType = type;
	_monsterTypeName = Protocol::MonsterType_Name(type);

	const MonsterDef& def = MonsterData::Get().Find(type);

	SetFootprint(def.footprintTiles, 1);					// 길찾기 NavGrid 번들링 (타일)
	SetCollisionBox(def.collisionCells, def.collisionCells);	// 벽 충돌 박스 (셀, 위치 중심)
	SetRadius(def.radius);								// 원형 충돌 반경 (투사체 명중 / 쿼드트리)
	SetMaxHp(def.maxHp);
	SetHp(def.maxHp);
	SetAttackPower(def.attackPower);
	SetMoveSpeedCells(def.moveSpeedCells);
}
