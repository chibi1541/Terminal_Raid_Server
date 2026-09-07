#include "pch.h"
#include "Game/Projectile.h"
#include "Game/ObjectIdGenerator.h"

Projectile::Projectile()
{
	SetObjType(Protocol::OBJECT_PROJECTILE);

	// objectId 상위 16비트가 타입이므로 타입을 정한 직후에 발급한다. (Player 와 동일)
	SetObjId(ObjectIdGenerator::GenerateObjectId(GetObjType()));

	// 체력 개념은 없지만 IsAlive() 로 살아있어야 룸 로직에 참여한다.
	SetMaxHp(1);
	SetHp(1);
	SetRadius(1);	// 3x3 (지름 3)
}

void Projectile::FillObjectInfo(Protocol::ObjectInfo* info)
{
	GameObject::FillObjectInfo(info);

	// 임의 각도 속도 - 클라가 이걸로 스폰 스냅샷을 받아 데드레커닝한다.
	Protocol::CreatureState* state = info->mutable_state();
	state->set_velsubx(Movement().velSubX);
	state->set_velsuby(Movement().velSubY);
}

void Projectile::LaunchVec(int32 velSubX, int32 velSubY, uint64 ownerId,
						   int32 rangeCells, uint64 roomTickNow, int32 lifetimeTicks)
{
	MovementComponent& m = Movement();

	m.ClearPath();
	m.dir = Protocol::DIR_NONE;
	m.speed = 0;
	m.velSubX = velSubX;
	m.velSubY = velSubY;
	m.state = (velSubX != 0 || velSubY != 0) ? MoveState::Moving : MoveState::Idle;
	m.dirty = true;

	_ownerId = ownerId;
	_originFpX = m.fpX;
	_originFpY = m.fpY;

	if (rangeCells > 0)
	{
		const int64 rangeSub = static_cast<int64>(rangeCells) * POS_SCALE;
		_maxDistSqSub = rangeSub * rangeSub;
	}
	else
	{
		_maxDistSqSub = 0;
	}

	_expireTick = roomTickNow + static_cast<uint64>((lifetimeTicks > 0) ? lifetimeTicks : 1);
}

void Projectile::Launch(Protocol::DirectionType dir, int32 cellsPerSec,
						uint64 roomTickNow, int32 lifetimeTicks)
{
	int32 ux = 0;
	int32 uy = 0;
	switch (dir)
	{
	case Protocol::DIR_LEFT:			ux = -1; break;
	case Protocol::DIR_RIGHT:		ux =  1; break;
	case Protocol::DIR_UP:			uy = -1; break;
	case Protocol::DIR_DOWN:		uy =  1; break;
	case Protocol::DIR_UP_LEFT:		ux = -1; uy = -1; break;
	case Protocol::DIR_UP_RIGHT:	ux =  1; uy = -1; break;
	case Protocol::DIR_DOWN_LEFT:	ux = -1; uy =  1; break;
	case Protocol::DIR_DOWN_RIGHT:	ux =  1; uy =  1; break;
	default: break;
	}

	const int32 speed = (cellsPerSec > 0) ? cellsPerSec : DEFAULT_MOVE_SPEED_CELLS;
	int32 velX = ux * speed * POS_SCALE;
	int32 velY = uy * speed * POS_SCALE;

	// 대각 보정 (Room 이동과 동일한 181/256)
	if (ux != 0 && uy != 0)
	{
		velX = velX * MoveMath::DIAG_NUM / MoveMath::DIAG_DEN;
		velY = velY * MoveMath::DIAG_NUM / MoveMath::DIAG_DEN;
	}

	LaunchVec(velX, velY, 0, 0, roomTickNow, lifetimeTicks);
}

bool Projectile::IsOutOfRange() const
{
	if (_maxDistSqSub == 0)
		return false;

	const MovementComponent& m = Movement();
	const int64 dx = static_cast<int64>(m.fpX) - _originFpX;
	const int64 dy = static_cast<int64>(m.fpY) - _originFpY;

	return (dx * dx + dy * dy) > _maxDistSqSub;
}
