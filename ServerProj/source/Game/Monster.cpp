#include "pch.h"
#include "Game/Monster.h"
#include "Game/ObjectIdGenerator.h"

namespace
{
	// 아직 몬스터 데이터 테이블이 없어 이름별 고정값만 임시로 둔다.
	// 실제 몬스터 데이터 테이블이 생기면 그쪽으로 옮길 것.
	int32 LookupFootprintTilesWide(const string& monsterTypeName)
	{
		static const HashMap<string, int32> table =
		{
			{ "goblin", 2 },
			{ "orc", 3 },
			{ "troll", 4 },
			{ "ogre", 5 },
			{ "dragon", 6 },
		};

		auto it = table.find(monsterTypeName);
		return (it != table.end()) ? it->second : 2;	// 모르는 이름이면 기본 2
	}
}

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
	SetFootprint(LookupFootprintTilesWide(name), 1);
}
