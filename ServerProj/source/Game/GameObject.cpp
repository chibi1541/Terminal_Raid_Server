#include "pch.h"
#include "Game/GameObject.h"

void GameObject::SetHp(int32 hp)
{
	if (hp < 0)
		hp = 0;
	else if (hp > _maxHp)
		hp = _maxHp;

	_hp = hp;
}

void GameObject::SetMaxHp(int32 maxHp)
{
	_maxHp = (maxHp > 0) ? maxHp : 1;

	// 최대치가 줄면 현재 체력도 같이 눌러준다.
	if (_hp > _maxHp)
		_hp = _maxHp;
}

bool GameObject::ApplyDamage(int32 damage)
{
	if (damage <= 0 || IsAlive() == false)
		return false;

	SetHp(GetHp() - damage);
	return IsAlive() == false;
}

void GameObject::SyncCellFromFixed()
{
	_pos.set_x(_move.fpX >> POS_SHIFT);
	_pos.set_y(_move.fpY >> POS_SHIFT);
}

void GameObject::FillObjectInfo(Protocol::ObjectInfo* info)
{
	// 개체 타입은 objectId 상위 16비트에 들어 있으므로 따로 싣지 않는다.
	info->set_objectid(_objectId);

	Protocol::CreatureState* state = info->mutable_state();
	state->mutable_pos()->CopyFrom(_pos);
	state->set_hp(_hp);
	state->set_maxhp(_maxHp);

	// 이동 상태도 스냅샷에 실어야 늦게 접속한 클라가 움직이는 액터를 바로 예측한다.
	state->set_dir(_move.dir);
	state->set_speed(_move.EffectiveSpeed());
}

Bounds GameObject::GetBounds() const
{
	return Bounds::FromCircle(_pos.x(), _pos.y(), _radius);
}

bool GameObject::OverlapsCircle(int32 centerX, int32 centerY, int32 radius) const
{
	const int32 dx = _pos.x() - centerX;
	const int32 dy = _pos.y() - centerY;
	const int32 sum = _radius + radius;

	return (dx * dx + dy * dy) <= (sum * sum);
}
