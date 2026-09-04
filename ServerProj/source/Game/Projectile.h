#pragma once
#include "Game/GameObject.h"

/*-----------
	Projectile

	직진 투사체. 이동 시스템을 눈으로 보며 디버깅하기 위한, 가장 단순하게 움직이는 개체다.

	- 발사 방향(8방향)으로 등속 직진한다. 길찾기도, 조향도 없다.
	- Room::UpdateMovement 가 다른 액터와 똑같이 적분한다. (별도 이동 코드 없음)
	- 벽에 닿으면 IntegrateActor 가 멈춰 세우고, 그 다음 틱에 Room 이 걷어낸다.
	- 그 전이라도 수명(_expireTick)이 다하면 걷어낸다.
------------*/

class Projectile : public GameObject
{
public:
	Projectile();

	// dir 방향으로 cellsPerSec 셀/초 직진시킨다. cellsPerSec <= 0 이면 기본 속도.
	// roomTickNow 는 발사 시점의 룸 틱, lifetimeTicks 틱 뒤에 소멸 예약한다.
	void	Launch(Protocol::DirectionType dir, int32 cellsPerSec,
				   uint64 roomTickNow, int32 lifetimeTicks);

	uint64	GetExpireTick() const { return _expireTick; }

private:
	uint64	_expireTick = 0;
};

using ProjectileRef = shared_ptr<Projectile>;
