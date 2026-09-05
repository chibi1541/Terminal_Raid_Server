#include "pch.h"
#include "Game/Monster.h"
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
