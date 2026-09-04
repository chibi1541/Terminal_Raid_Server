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
	SetRadius(0);
}

void Projectile::Launch(Protocol::DirectionType dir, int32 cellsPerSec,
						uint64 roomTickNow, int32 lifetimeTicks)
{
	MovementComponent& m = Movement();

	m.ClearPath();
	m.dir = dir;
	m.speed = (cellsPerSec > 0) ? cellsPerSec * POS_SCALE : 0;	// 0 => 기본 속도
	m.state = (dir == Protocol::DIR_NONE) ? MoveState::Idle : MoveState::Moving;
	m.dirty = true;

	_expireTick = roomTickNow + static_cast<uint64>((lifetimeTicks > 0) ? lifetimeTicks : 1);
}
