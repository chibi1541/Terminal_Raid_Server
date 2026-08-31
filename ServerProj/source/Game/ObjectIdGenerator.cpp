#include "pch.h"
#include "Game/ObjectIdGenerator.h"

uint64 ObjectIdGenerator::GenerateObjectId(Protocol::ObjectType type)
{
	// 모든 타입이 카운터 하나를 공유한다. 타입별로 나누면
	// 상위 비트만 다른 같은 일련번호가 생겨서 로그를 읽기 나빠진다.
	static Atomic<uint64> s_nextCount = 1;

	const uint64 count = s_nextCount.fetch_add(1) & OBJECT_COUNT_MASK;
	const uint64 typeBits = (static_cast<uint64>(type) << OBJECT_TYPE_SHIFT) & OBJECT_TYPE_MASK;

	return typeBits | count;
}

Protocol::ObjectType ObjectIdGenerator::GetObjectType(uint64 objectId)
{
	return static_cast<Protocol::ObjectType>((objectId & OBJECT_TYPE_MASK) >> OBJECT_TYPE_SHIFT);
}
