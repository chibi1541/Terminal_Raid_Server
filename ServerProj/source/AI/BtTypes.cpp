#include "pch.h"
#include "AI/BtTypes.h"

int64 BbValue::ToInt() const
{
	switch (type)
	{
	case BbType::Int:	return asInt;
	case BbType::Float:	return static_cast<int64>(asFloat);
	case BbType::Bool:	return asBool ? 1 : 0;
	}

	return 0;
}

float BbValue::ToFloat() const
{
	switch (type)
	{
	case BbType::Int:	return static_cast<float>(asInt);
	case BbType::Float:	return asFloat;
	case BbType::Bool:	return asBool ? 1.0f : 0.0f;
	}

	return 0.0f;
}

bool BbValue::ToBool() const
{
	switch (type)
	{
	case BbType::Int:	return asInt != 0;
	case BbType::Float:	return asFloat != 0.0f;
	case BbType::Bool:	return asBool;
	}

	return false;
}

std::wstring BbValue::Describe() const
{
	WCHAR buffer[64];

	switch (type)
	{
	case BbType::Int:
		::swprintf_s(buffer, L"%lld", asInt);
		break;

	case BbType::Float:
		::swprintf_s(buffer, L"%.3f", asFloat);
		break;

	case BbType::Bool:
		::swprintf_s(buffer, L"%s", asBool ? L"true" : L"false");
		break;

	default:
		::swprintf_s(buffer, L"?");
		break;
	}

	return buffer;
}

const WCHAR* ToString(BtStatus status)
{
	switch (status)
	{
	case BtStatus::Failure:	return L"Failure";
	case BtStatus::Success:	return L"Success";
	case BtStatus::Running:	return L"Running";
	}

	return L"?";
}

const WCHAR* ToString(BtNodeType type)
{
	switch (type)
	{
	case BtNodeType::Sequence:	return L"Sequence";
	case BtNodeType::Selector:	return L"Selector";
	case BtNodeType::Inverter:	return L"Inverter";
	case BtNodeType::Succeeder:	return L"Succeeder";
	case BtNodeType::Leaf:		return L"Leaf";
	}

	return L"?";
}
