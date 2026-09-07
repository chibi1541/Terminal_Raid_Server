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

	// 몬스터 전용 직렬화 필드 없음 - objectType(OBJECT_MONSTER)으로 클라가 이미 구분 가능
}

void Monster::SetMonsterTypeName(const string& name)
{
	_monsterTypeName = name;

	// 이름으로 MonsterData 테이블을 조회해 길찾기 풋프린트 / 충돌 박스 / 반경 / 체력을 정한다.
	const MonsterDef& def = MonsterData::Get().Find(name);

	SetFootprint(def.footprintTiles, 1);				// 길찾기 NavGrid 번들링 (타일)
	SetCollisionBox(def.collisionCells, def.collisionCells);	// 벽 충돌 박스 (셀, 위치 중심)
	SetRadius(def.radius);								// 원형 충돌 반경 (투사체 명중 / 쿼드트리)
	SetMaxHp(def.maxHp);
	SetHp(def.maxHp);
}
