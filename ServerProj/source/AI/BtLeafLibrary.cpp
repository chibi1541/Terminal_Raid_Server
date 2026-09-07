#include "pch.h"
#include "AI/BtNodeRegistry.h"
#include "AI/BehaviorTree.h"
#include "Room.h"
#include "Game/GameObject.h"
#include "Game/ObjectIdGenerator.h"
#include "Game/ProjectileData.h"

#include <cmath>

/*-----------------
	BtLeafLibrary

	두 묶음이 있다.
	  1. 유틸 리프 : Wait / Log / AlwaysSucceed / AlwaysFail / SetBlackboard / CheckBlackboard.
	  2. 게임 로직 리프 : FindTargetInRadius / MoveToTarget / TargetInRange / AttackTarget.
	     몬스터의 대기 -> 추적 -> 공격 AI (Data/AI/monster_basic.canvas) 가 쓴다.
	     전부 context.room / context.self 만 얇게 감싼다 (Room::QueryCircle / OrderMoveTo /
	     DealDamage / NotifyAttackStart).

	전부 Execute 가 const 다. 리프는 공유물이라 자기 안에 진행 상태를 쌓을 수 없고,
	생성자에서 받은 값(슬롯 인덱스, 상수)만 불변으로 들고 있다.
	타이머 / 타겟 id 같은 개체별 상태는 전부 블랙보드 슬롯에 둔다.
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

	/*================ 게임 로직 리프 (몬스터 AI) ================*/

	// 두 셀 사이 거리 제곱 (정수). sqrt 안 쓰고 range*range 와 비교한다.
	int64 CellDistSq(const GameObject& a, const GameObject& b)
	{
		const int64 dx = static_cast<int64>(a.GetPosX()) - b.GetPosX();
		const int64 dy = static_cast<int64>(a.GetPosY()) - b.GetPosY();
		return dx * dx + dy * dy;
	}

	// self -> target 방향을 8방향 enum 으로. 콘솔 y 는 아래로 증가 → dy<0 이 화면상 위(UP).
	Protocol::DirectionType DirTo8(const GameObject& self, const GameObject& target)
	{
		const int32 dx = target.GetPosX() - self.GetPosX();
		const int32 dy = target.GetPosY() - self.GetPosY();
		const int sx = (dx > 0) - (dx < 0);
		const int sy = (dy > 0) - (dy < 0);

		if (sx == 0 && sy == 0)	return Protocol::DIR_NONE;
		if (sx == 0)	return (sy < 0) ? Protocol::DIR_UP : Protocol::DIR_DOWN;
		if (sy == 0)	return (sx < 0) ? Protocol::DIR_LEFT : Protocol::DIR_RIGHT;
		if (sx < 0)		return (sy < 0) ? Protocol::DIR_UP_LEFT : Protocol::DIR_DOWN_LEFT;
		return (sy < 0) ? Protocol::DIR_UP_RIGHT : Protocol::DIR_DOWN_RIGHT;
	}

	// 블랙보드에 담긴 targetId 로 살아있는 대상을 잠근다. 없으면 nullptr.
	GameObject* ResolveTarget(BtContext& context, int32 targetSlot)
	{
		if (context.room == nullptr || targetSlot < 0)
			return nullptr;

		const uint64 targetId = static_cast<uint64>(context.blackboard->GetInt(targetSlot));
		if (targetId == 0)
			return nullptr;

		GameObjectRef target = context.room->Find(targetId);
		if (target == nullptr || target->IsAlive() == false)
			return nullptr;

		return target.get();
	}

	/*--- FindTargetInRadius : 반경 안의 가장 가까운 플레이어를 targetId 슬롯에 기록 ---*/
	//
	// 찾으면 Success (+ distKey 에 거리), 못 찾으면 targetId=0 으로 지우고 Failure.
	// 몬스터 -> 플레이어 고정 (진영 개념이 생기면 파라미터로).
	class FindTargetInRadiusLeaf : public BtLeaf
	{
	public:
		FindTargetInRadiusLeaf(int32 radius, int32 targetSlot, int32 distSlot)
			: _radius(radius), _targetSlot(targetSlot), _distSlot(distSlot) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			if (context.room == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			Vector<GameObject*> hits;
			context.room->QueryCircle(context.self->GetPosX(), context.self->GetPosY(), _radius, OUT hits);

			GameObject* best = nullptr;
			int64 bestDistSq = 0;

			for (GameObject* obj : hits)
			{
				if (obj == nullptr || obj == context.self)
					continue;
				if (obj->GetObjType() != Protocol::OBJECT_PLAYER || obj->IsAlive() == false)
					continue;

				const int64 distSq = CellDistSq(*context.self, *obj);
				if (best == nullptr || distSq < bestDistSq)
				{
					best = obj;
					bestDistSq = distSq;
				}
			}

			if (best == nullptr)
			{
				context.blackboard->SetInt(_targetSlot, 0);
				return BtStatus::Failure;
			}

			context.blackboard->SetInt(_targetSlot, static_cast<int64>(best->GetObjId()));
			if (_distSlot >= 0)
				context.blackboard->SetFloat(_distSlot, ::sqrtf(static_cast<float>(bestDistSq)));

			return BtStatus::Success;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(radius=%d targetSlot=%d distSlot=%d)", _radius, _targetSlot, _distSlot);
			return buffer;
		}

	private:
		const int32 _radius;
		const int32 _targetSlot;
		const int32 _distSlot;
	};

	/*--- MoveToTarget : targetId 로 JPS 경로를 깔고 추종. 도착하면 Success ---*/
	//
	// repathInterval 마다 (또는 경로가 없으면 즉시) 대상의 현재 셀로 다시 경로를 짠다.
	// arriveRange 안에 들면 Success, giveUpRange 밖으로 벌어지면 Failure, 그 외 Running.
	class MoveToTargetLeaf : public BtLeaf
	{
	public:
		MoveToTargetLeaf(int32 targetSlot, int32 timerSlot, float repathInterval,
						 int32 arriveRange, int32 giveUpRange)
			: _targetSlot(targetSlot), _timerSlot(timerSlot), _repathInterval(repathInterval)
			, _arriveRangeSq(static_cast<int64>(arriveRange) * arriveRange)
			, _giveUpRangeSq(static_cast<int64>(giveUpRange) * giveUpRange) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			GameObject* target = ResolveTarget(context, _targetSlot);
			if (target == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			const int64 distSq = CellDistSq(*context.self, *target);

			if (distSq <= _arriveRangeSq)
				return BtStatus::Success;	// 도착 - 상위 셀렉터가 공격 분기를 다시 본다

			if (_giveUpRangeSq > 0 && distSq > _giveUpRangeSq)
				return BtStatus::Failure;	// 너무 멀어짐 - 추적 포기

			const float timer = context.blackboard->GetFloat(_timerSlot) + context.deltaTime;
			const bool noPath = (context.self->Movement().HasPath() == false);

			if (timer >= _repathInterval || noPath)
			{
				const bool ok = context.room->OrderMoveTo(
					context.self->GetObjId(), target->GetPosX(), target->GetPosY());
				context.blackboard->SetFloat(_timerSlot, 0.0f);

				// 경로를 못 짰고 지금도 경로가 없다 = 도달 불가. 무한 Running 방지.
				if (ok == false && context.self->Movement().HasPath() == false)
					return BtStatus::Failure;
			}
			else
			{
				context.blackboard->SetFloat(_timerSlot, timer);
			}

			return BtStatus::Running;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(targetSlot=%d timerSlot=%d repath=%.2f)",
				_targetSlot, _timerSlot, _repathInterval);
			return buffer;
		}

	private:
		const int32 _targetSlot;
		const int32 _timerSlot;
		const float _repathInterval;
		const int64 _arriveRangeSq;
		const int64 _giveUpRangeSq;
	};

	/*--- TargetInRange : targetId 대상이 range 셀 안에 있으면 Success ---*/
	class TargetInRangeLeaf : public BtLeaf
	{
	public:
		TargetInRangeLeaf(int32 targetSlot, int32 range)
			: _targetSlot(targetSlot), _rangeSq(static_cast<int64>(range) * range) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			GameObject* target = ResolveTarget(context, _targetSlot);
			if (target == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			return (CellDistSq(*context.self, *target) <= _rangeSq)
				? BtStatus::Success : BtStatus::Failure;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[96];
			::swprintf_s(buffer, L"(targetSlot=%d)", _targetSlot);
			return buffer;
		}

	private:
		const int32 _targetSlot;
		const int64 _rangeSq;
	};

	/*--- AttackTarget : 쿨다운이 차면 S_ATTACK_START + DealDamage. 항상 Success (대상 유효 시) ---*/
	//
	// damage <= 0 이면 self 의 공격력(GameObject::GetAttackPower)을 쓴다.
	// 쿨다운 경과는 timerSlot 에 쌓는다 (Wait 와 같은 방식).
	class AttackTargetLeaf : public BtLeaf
	{
	public:
		AttackTargetLeaf(int32 targetSlot, int32 timerSlot, float cooldown, int32 damage)
			: _targetSlot(targetSlot), _timerSlot(timerSlot), _cooldown(cooldown), _damage(damage) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			GameObject* target = ResolveTarget(context, _targetSlot);
			if (target == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			const float timer = context.blackboard->GetFloat(_timerSlot) + context.deltaTime;

			if (timer >= _cooldown)
			{
				const int32 dmg = (_damage > 0) ? _damage : context.self->GetAttackPower();

				context.room->NotifyAttackStart(context.self->GetObjId(), DirTo8(*context.self, *target));
				context.room->DealDamage(context.self->GetObjId(), target->GetObjId(), dmg);

				context.blackboard->SetFloat(_timerSlot, 0.0f);
			}
			else
			{
				context.blackboard->SetFloat(_timerSlot, timer);
			}

			return BtStatus::Success;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[128];
			::swprintf_s(buffer, L"(targetSlot=%d timerSlot=%d cooldown=%.2f damage=%d)",
				_targetSlot, _timerSlot, _cooldown, _damage);
			return buffer;
		}

	private:
		const int32 _targetSlot;
		const int32 _timerSlot;
		const float _cooldown;
		const int32 _damage;
	};

	/*--- TargetOutOfRange : targetId 대상이 range 셀 "밖"이면 Success (대상 없으면 Failure) ---*/
	// TargetInRange 의 거울. 보스 리포지션 게이트("플레이어가 멀면 조금 이동")에 쓴다.
	class TargetOutOfRangeLeaf : public BtLeaf
	{
	public:
		TargetOutOfRangeLeaf(int32 targetSlot, int32 range)
			: _targetSlot(targetSlot), _rangeSq(static_cast<int64>(range) * range) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			GameObject* target = ResolveTarget(context, _targetSlot);
			if (target == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			return (CellDistSq(*context.self, *target) > _rangeSq)
				? BtStatus::Success : BtStatus::Failure;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[96];
			::swprintf_s(buffer, L"(targetSlot=%d)", _targetSlot);
			return buffer;
		}

	private:
		const int32 _targetSlot;
		const int64 _rangeSq;
	};

	/*--- SelfHpBelow : self 의 hp 가 maxHp 의 fraction 이하이면 Success ---*/
	// 보스 페이즈 전환 게이트 (hp 50% 이하 -> 2차 패턴).
	class SelfHpBelowLeaf : public BtLeaf
	{
	public:
		explicit SelfHpBelowLeaf(float fraction) : _fraction(fraction) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			if (context.self == nullptr || context.self->GetMaxHp() <= 0)
				return BtStatus::Failure;

			const float ratio = static_cast<float>(context.self->GetHp())
				/ static_cast<float>(context.self->GetMaxHp());

			return (ratio <= _fraction) ? BtStatus::Success : BtStatus::Failure;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[64];
			::swprintf_s(buffer, L"(fraction=%.2f)", _fraction);
			return buffer;
		}

	private:
		const float _fraction;
	};

	/*--- FireRadialBurst : self 중심 rays 방향으로 투사체를 한 번에 발사 ---*/
	//
	// 보스 패턴용. burstIndexKey 슬롯의 값으로 패턴 전체를 회전시킨다 (선형 스윕):
	//   offset = (2π / rays) * burstIndex / totalBursts
	// totalBursts 번 쏘면 정확히 한 칸(2π/rays) 회전해 첫 발과 맞물린다.
	// secondaryEvery > 0 이면 burstIndex % secondaryEvery == 0 인 발사에서 secondaryType 을
	// secondaryRays 갈래로 함께 쏜다 (primary 사이에 절반 오프셋으로 끼워 "섞어서").
	// 매 호출 burstIndex 를 1 증가시키고 항상 Success.
	class FireRadialBurstLeaf : public BtLeaf
	{
	public:
		FireRadialBurstLeaf(Protocol::ProjectileType type, int32 rays, int32 burstSlot, int32 totalBursts,
							Protocol::ProjectileType secondaryType, int32 secondaryRays, int32 secondaryEvery)
			: _type(type), _rays(rays), _burstSlot(burstSlot)
			, _totalBursts((totalBursts > 0) ? totalBursts : 1)
			, _secondaryType(secondaryType), _secondaryRays(secondaryRays)
			, _secondaryEvery(secondaryEvery) {}

		virtual BtStatus Execute(BtContext& context) const override
		{
			if (context.room == nullptr || context.self == nullptr)
				return BtStatus::Failure;

			const int64 burstIndex = context.blackboard->GetInt(_burstSlot);

			const int32 cx = context.self->GetPosX();
			const int32 cy = context.self->GetPosY();
			const uint64 selfId = context.self->GetObjId();

			FireRing(context, selfId, cx, cy, _type, _rays, burstIndex, _totalBursts, 0.0);

			if (_secondaryEvery > 0 && _secondaryRays > 0
				&& _secondaryType != Protocol::Projectile_None
				&& (burstIndex % _secondaryEvery) == 0)
			{
				// 절반 오프셋(0.5)으로 primary 사이에 끼운다.
				FireRing(context, selfId, cx, cy, _secondaryType, _secondaryRays, burstIndex, _totalBursts, 0.5);
			}

			// 시전 모션 트리거 (방향은 아래쪽 고정 - 방사형이라 조준 방향 개념이 없다).
			context.room->NotifyAttackStart(selfId, Protocol::DIR_DOWN);

			context.blackboard->SetInt(_burstSlot, burstIndex + 1);
			return BtStatus::Success;
		}

		virtual std::wstring Describe() const override
		{
			WCHAR buffer[160];
			::swprintf_s(buffer, L"(type=%d rays=%d total=%d sec=%d secRays=%d secEvery=%d)",
				static_cast<int32>(_type), _rays, _totalBursts,
				static_cast<int32>(_secondaryType), _secondaryRays, _secondaryEvery);
			return buffer;
		}

	private:
		// ratioOffset : gap 의 몇 배만큼 링 전체를 더 돌릴지 (secondary 를 primary 사이에 끼울 때 0.5).
		static void FireRing(BtContext& context, uint64 selfId, int32 cx, int32 cy,
							 Protocol::ProjectileType type, int32 rays, int64 burstIndex,
							 int32 totalBursts, double ratioOffset)
		{
			const double gap = 6.283185307179586 / static_cast<double>(rays);
			const double sweep = gap * static_cast<double>(burstIndex) / static_cast<double>(totalBursts);
			const double base = sweep + gap * ratioOffset;

			for (int32 i = 0; i < rays; i++)
			{
				const double a = base + gap * static_cast<double>(i);
				context.room->SpawnProjectileAimed(selfId, cx, cy,
					static_cast<float>(::cos(a)), static_cast<float>(::sin(a)), type);
			}
		}

		const Protocol::ProjectileType	_type;
		const int32						_rays;
		const int32						_burstSlot;
		const int32						_totalBursts;
		const Protocol::ProjectileType	_secondaryType;
		const int32						_secondaryRays;
		const int32						_secondaryEvery;
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

	/*--- 게임 로직 리프 (monster_basic.canvas) ---*/

	Register("FindTargetInRadius",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 targetSlot = ResolveSlot(params, tree, "targetKey");
			if (targetSlot < 0)
				return nullptr;	// targetKey 미선언

			const int32 distSlot = ResolveSlot(params, tree, "distKey");	// 선택 (-1 허용)

			return new FindTargetInRadiusLeaf(
				static_cast<int32>(params.GetFloat("radius", 60.0f)), targetSlot, distSlot);
		});

	Register("MoveToTarget",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 targetSlot = ResolveSlot(params, tree, "targetKey");
			const int32 timerSlot = ResolveSlot(params, tree, "repathTimer");
			if (targetSlot < 0 || timerSlot < 0)
				return nullptr;

			return new MoveToTargetLeaf(
				targetSlot, timerSlot,
				params.GetFloat("repathInterval", 0.4f),
				static_cast<int32>(params.GetFloat("arriveRange", 5.0f)),
				static_cast<int32>(params.GetFloat("giveUpRange", 0.0f)));
		});

	Register("TargetInRange",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 targetSlot = ResolveSlot(params, tree, "targetKey");
			if (targetSlot < 0)
				return nullptr;

			return new TargetInRangeLeaf(
				targetSlot, static_cast<int32>(params.GetFloat("range", 6.0f)));
		});

	Register("AttackTarget",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 targetSlot = ResolveSlot(params, tree, "targetKey");
			const int32 timerSlot = ResolveSlot(params, tree, "cooldownTimer");
			if (targetSlot < 0 || timerSlot < 0)
				return nullptr;

			return new AttackTargetLeaf(
				targetSlot, timerSlot,
				params.GetFloat("cooldown", 1.0f),
				static_cast<int32>(params.GetInt("damage", 0)));
		});

	Register("TargetOutOfRange",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 targetSlot = ResolveSlot(params, tree, "targetKey");
			if (targetSlot < 0)
				return nullptr;

			return new TargetOutOfRangeLeaf(
				targetSlot, static_cast<int32>(params.GetFloat("range", 40.0f)));
		});

	Register("FireRadialBurst",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			const int32 burstSlot = ResolveSlot(params, tree, "burstIndexKey");
			if (burstSlot < 0)
				return nullptr;

			Protocol::ProjectileType type = Protocol::Projectile_None;
			Protocol::ProjectileType_Parse(params.GetString("projectileType", "Projectile_Fire"), &type);
			if (type == Protocol::Projectile_None)
				return nullptr;

			// 선택 : hp 낮을 때 섞어 쓰는 2차 투사체.
			Protocol::ProjectileType secondaryType = Protocol::Projectile_None;
			Protocol::ProjectileType_Parse(params.GetString("secondaryType", ""), &secondaryType);

			return new FireRadialBurstLeaf(type,
				static_cast<int32>(params.GetInt("rays", 16)), burstSlot,
				static_cast<int32>(params.GetInt("totalBursts", 8)),
				secondaryType,
				static_cast<int32>(params.GetInt("secondaryRays", 0)),
				static_cast<int32>(params.GetInt("secondaryEvery", 0)));
		});

	Register("SelfHpBelow",
		[](const BtParams& params, const BehaviorTree& tree) -> BtLeaf*
		{
			return new SelfHpBelowLeaf(params.GetFloat("fraction", 0.5f));
		});
}
