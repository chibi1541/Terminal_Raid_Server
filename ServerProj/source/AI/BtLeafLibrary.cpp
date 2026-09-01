#include "pch.h"
#include "AI/BtNodeRegistry.h"
#include "AI/BehaviorTree.h"
#include "Game/GameObject.h"

/*-----------------
	BtLeafLibrary

	검증용 더미 리프들. 실제 게임 로직 리프(FindTarget / MoveToTarget / Attack)는 다음 단계다.

	전부 Execute 가 const 다. 리프는 공유물이라 자기 안에 진행 상태를 쌓을 수 없고,
	생성자에서 받은 값(슬롯 인덱스, 상수)만 불변으로 들고 있다.
------------------*/

namespace
{
	// 파라미터에 적힌 블랙보드 키를 슬롯 인덱스로 바꾼다.
	// 선언 안 된 키면 -1 이 나오고 팩토리가 nullptr 을 돌려주게 만든다.
	int32 ResolveSlot(const BtParams& params, const BehaviorTree& tree, const char* paramName)
	{
		const string key = params.GetString(paramName);

		if (key.empty())
			return -1;

		return tree.FindBlackboardSlot(key);
	}

	/*--- Wait : duration 초가 찰 때까지 Running ---*/
	//
	// 경과 시간을 리프가 아니라 블랙보드 슬롯에 쌓는다.
	// 같은 Wait 리프를 100 마리가 동시에 밟아도 서로 간섭하지 않는 이유가 이것이다.
	class WaitLeaf : public BtLeaf
	{
	public:
		WaitLeaf(float duration, int32 timerSlot)
			: _duration(duration), _timerSlot(timerSlot) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			const float elapsed = context.blackboard->GetFloat(_timerSlot) + context.deltaTime;

			if (elapsed >= _duration)
			{
				context.blackboard->SetFloat(_timerSlot, 0.0f);
				return BtStatus::Success;
			}

			context.blackboard->SetFloat(_timerSlot, elapsed);
			return BtStatus::Running;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(duration=%.2f timerSlot=%d)", _duration, _timerSlot);
			return buffer;
		}

	private:
		const float _duration;
		const int32 _timerSlot;
	};

	/*--- Log : 로그 한 줄 찍고 Success ---*/
	class LogLeaf : public BtLeaf
	{
	public:
		explicit LogLeaf(const string& message) : _message(message) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			const uint64 objectId = (context.self != nullptr) ? context.self->GetObjId() : 0;

			LOG_INFO(L"[bt] objectId=%llu : %hs", objectId, _message.c_str());
			return BtStatus::Success;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[256];
			::swprintf_s(buffer, L"(message=%hs)", _message.c_str());
			return buffer;
		}

	private:
		const string _message;
	};

	/*--- AlwaysSucceed / AlwaysFail ---*/
	class ConstantLeaf : public BtLeaf
	{
	public:
		explicit ConstantLeaf(BtStatus result) : _result(result) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			return _result;
		}

	private:
		const BtStatus _result;
	};

	/*--- SetBlackboard : 슬롯에 값 하나 쓰고 Success ---*/
	class SetBlackboardLeaf : public BtLeaf
	{
	public:
		SetBlackboardLeaf(int32 slot, const BbValue& value) : _slot(slot), _value(value) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			context.blackboard->Set(_slot, _value);
			return BtStatus::Success;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(slot=%d value=%s)", _slot, _value.Describe().c_str());
			return buffer;
		}

	private:
		const int32		_slot;
		const BbValue	_value;
	};

	/*--- CheckBlackboard : 비교 결과를 Success / Failure 로 ---*/
	class CheckBlackboardLeaf : public BtLeaf
	{
	public:
		enum class Op : uint8 { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

		CheckBlackboardLeaf(int32 slot, Op op, float value)
			: _slot(slot), _op(op), _value(value) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			const float current = context.blackboard->GetFloat(_slot);
			bool result = false;

			switch (_op)
			{
			case Op::Equal:			result = (current == _value); break;
			case Op::NotEqual:		result = (current != _value); break;
			case Op::Less:			result = (current < _value);  break;
			case Op::LessEqual:		result = (current <= _value); break;
			case Op::Greater:		result = (current > _value);  break;
			case Op::GreaterEqual:	result = (current >= _value); break;
			}

			return result ? BtStatus::Success : BtStatus::Failure;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(slot=%d op=%d value=%.2f)", _slot, static_cast<int32>(_op), _value);
			return buffer;
		}

		// 기호와 단어를 모두 받는다. 캔버스에 부등호를 쓰기 불편한 경우가 있다.
		static bool ParseOp(const string& text, OUT Op& outOp)
		{
			if (text == "==" || text == "eq") { outOp = Op::Equal;        return true; }
			if (text == "!=" || text == "ne") { outOp = Op::NotEqual;     return true; }
			if (text == "<"  || text == "lt") { outOp = Op::Less;         return true; }
			if (text == "<=" || text == "le") { outOp = Op::LessEqual;    return true; }
			if (text == ">"  || text == "gt") { outOp = Op::Greater;      return true; }
			if (text == ">=" || text == "ge") { outOp = Op::GreaterEqual; return true; }

			return false;
		}

	private:
		const int32	_slot;
		const Op	_op;
		const float	_value;
	};
}

void BtNodeRegistry::RegisterBuiltins()
{
	Register("Wait",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 slot = ResolveSlot(params, tree, "timer");

			// timer 키를 선언하지 않았으면 여기서 막는다.
			// 안 막으면 슬롯 -1 에 쓰다가 조용히 아무 일도 안 일어난다.
			if (slot < 0)
				return nullptr;

			return new WaitLeaf(params.GetFloat("duration", 1.0f), slot);
		});

	Register("Log",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			return new LogLeaf(params.GetString("message", "log"));
		});

	Register("AlwaysSucceed",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			return new ConstantLeaf(BtStatus::Success);
		});

	Register("AlwaysFail",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			return new ConstantLeaf(BtStatus::Failure);
		});

	Register("SetBlackboard",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 slot = ResolveSlot(params, tree, "key");

			if (slot < 0)
				return nullptr;

			const string raw = params.GetString("value", "0");

			BbValue value;
			if (::_stricmp(raw.c_str(), "true") == 0)
				value = BbValue::MakeBool(true);
			else if (::_stricmp(raw.c_str(), "false") == 0)
				value = BbValue::MakeBool(false);
			else if (raw.find('.') != string::npos)
				value = BbValue::MakeFloat(static_cast<float>(::atof(raw.c_str())));
			else
				value = BbValue::MakeInt(::_atoi64(raw.c_str()));

			return new SetBlackboardLeaf(slot, value);
		});

	Register("CheckBlackboard",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 slot = ResolveSlot(params, tree, "key");

			if (slot < 0)
				return nullptr;

			CheckBlackboardLeaf::Op op = CheckBlackboardLeaf::Op::Equal;

			if (CheckBlackboardLeaf::ParseOp(params.GetString("op", "=="), OUT op) == false)
				return nullptr;

			return new CheckBlackboardLeaf(slot, op, params.GetFloat("value", 0.0f));
		});
}
