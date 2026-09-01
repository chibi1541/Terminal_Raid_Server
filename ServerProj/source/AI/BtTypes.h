#pragma once

/*-----------
	BtTypes

	비헤이비어 트리 공통 타입.
------------*/

enum class BtStatus : uint8
{
	Failure,
	Success,
	Running,
};

enum class BtNodeType : uint8
{
	Sequence,	// 자식을 앞에서부터, 하나라도 실패하면 실패
	Selector,	// 자식을 앞에서부터, 하나라도 성공하면 성공
	Inverter,	// 자식 하나. 성공과 실패를 뒤집는다
	Succeeder,	// 자식 하나. 실패해도 성공으로 바꾼다
	Leaf,		// 실제 일을 하는 노드. 레지스트리가 만든다
};

enum class BbType : uint8
{
	Int,
	Float,
	Bool,
};

/*-----------
	BbValue

	블랙보드 값 하나.

	union 으로 묶으면 24바이트를 8바이트로 줄일 수 있지만 그러지 않았다.
	키가 개체당 10개 남짓이라 아껴봐야 몇백 바이트고,
	타입을 헷갈려 엉뚱한 필드를 읽는 실수가 훨씬 비싸다.
------------*/

struct BbValue
{
	BbType	type = BbType::Int;
	int64	asInt = 0;
	float	asFloat = 0.0f;
	bool	asBool = false;

	static BbValue MakeInt(int64 value)
	{
		BbValue result;
		result.type = BbType::Int;
		result.asInt = value;
		return result;
	}

	static BbValue MakeFloat(float value)
	{
		BbValue result;
		result.type = BbType::Float;
		result.asFloat = value;
		return result;
	}

	static BbValue MakeBool(bool value)
	{
		BbValue result;
		result.type = BbType::Bool;
		result.asBool = value;
		return result;
	}

	// 타입이 달라도 최대한 읽어준다. 캔버스에서 3 이라고 쓴 값을 실수로 읽는 경우가 흔하다.
	int64	ToInt() const;
	float	ToFloat() const;
	bool	ToBool() const;

	std::wstring Describe() const;
};

const WCHAR* ToString(BtStatus status);
const WCHAR* ToString(BtNodeType type);
