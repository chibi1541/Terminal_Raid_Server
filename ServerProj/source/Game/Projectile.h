#pragma once
#include "Game/GameObject.h"

/*-----------
	Projectile

	등속 직진 투사체.

	- 발사 시 정해진 속도 벡터(velSub, 마우스 조준 방향)로 등속 직진한다. 조향 없음.
	- Room::UpdateMovement 가 m.velSub 분기로 적분한다 (8방향 dir 안 씀).
	- 벽에 닿으면 IntegrateVec 가 그 자리에 멈추고, 룸이 다음 정리에서 걷어낸다.
	- 사정거리(_maxDistSqSub)를 넘거나 수명(_expireTick)이 다해도 걷어낸다.
	- 대상 명중/데미지는 아직 없다 (_ownerId 만 미리 둔다).
------------*/

class Projectile : public GameObject
{
public:
	Projectile();

	virtual void FillObjectInfo(Protocol::ObjectInfo* info) override;

	// velSubX/Y : 서브유닛/초 속도 벡터. roomTickNow 발사 시점 룸 틱, lifetimeTicks 뒤 소멸 예약.
	// rangeCells <= 0 이면 사정거리 무제한 (수명만). damage : 명중 시 대상에게 줄 피해.
	void	LaunchVec(int32 velSubX, int32 velSubY, uint64 ownerId,
					  int32 rangeCells, uint64 roomTickNow, int32 lifetimeTicks, int32 damage);

	// 8방향 편의 버전 (proj 디버그 명령용). dir 을 velSub 로 바꿔 LaunchVec 에 위임.
	void	Launch(Protocol::DirectionType dir, int32 cellsPerSec,
				   uint64 roomTickNow, int32 lifetimeTicks, uint64 ownerId, int32 damage);

	// 종류. 클라가 이 타입으로 ProjectileData 를 조회해 애니메이션을 정한다. FillObjectInfo 가 싣는다.
	void					SetProjectileType(Protocol::ProjectileType type)	{ _type = type; }
	Protocol::ProjectileType	GetProjectileType() const						{ return _type; }

	uint64	GetExpireTick() const	{ return _expireTick; }
	uint64	GetOwnerId() const		{ return _ownerId; }
	int32	GetDamage() const		{ return _damage; }

	// 즉시 소멸 예약 (벽 히트 등). 다음 SweepExpiredProjectiles 가 걷어간다.
	void	MarkExpired()			{ _expireTick = 0; }

	// 발사 원점(고정소수점)에서 현재까지의 거리제곱이 사정거리 제곱을 넘었는가.
	bool	IsOutOfRange() const;

private:
	Protocol::ProjectileType	_type = Protocol::Projectile_Pellet;
	uint64	_expireTick = 0;
	uint64	_ownerId = 0;
	int32	_damage = 0;
	int32	_originFpX = 0;
	int32	_originFpY = 0;
	int64	_maxDistSqSub = 0;	// 0 = 무제한
};

using ProjectileRef = shared_ptr<Projectile>;
