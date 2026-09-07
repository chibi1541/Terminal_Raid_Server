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

	// 몬스터 전용 직렬화 필드 없음 - objectType(OBJECT_MONSTER)으로 클라가 이미 구분 가능.
	// (스폰 리팩터 후 MonsterType 을 여기 실어 보낼 예정)
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
