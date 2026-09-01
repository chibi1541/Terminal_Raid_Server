#include "pch.h"
#include "AI/Blackboard.h"

namespace
{
	// 슬롯이 어긋나면 조용히 0을 돌려주는 대신 눈에 띄게 만든다.
	const BbValue SInvalidValue;
}

void Blackboard::Init(const Vector<BbValue>& defaults)
{
	_values = defaults;
}

const BbValue& Blackboard::Get(int32 slot) const
{
	if (IsValidSlot(slot) == false)
		return SInvalidValue;

	return _values[slot];
}

int64 Blackboard::GetInt(int32 slot) const
{
	return Get(slot).ToInt();
}

float Blackboard::GetFloat(int32 slot) const
{
	return Get(slot).ToFloat();
}

bool Blackboard::GetBool(int32 slot) const
{
	return Get(slot).ToBool();
}

void Blackboard::Set(int32 slot, const BbValue& value)
{
	if (IsValidSlot(slot) == false)
		return;

	_values[slot] = value;
}

void Blackboard::SetInt(int32 slot, int64 value)
{
	Set(slot, BbValue::MakeInt(value));
}

void Blackboard::SetFloat(int32 slot, float value)
{
	Set(slot, BbValue::MakeFloat(value));
}

void Blackboard::SetBool(int32 slot, bool value)
{
	Set(slot, BbValue::MakeBool(value));
}
