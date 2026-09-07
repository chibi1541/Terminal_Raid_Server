#include "pch.h"
#include "Game/Player.h"
#include "Game/ObjectIdGenerator.h"

Player::Player()
{
	SetObjType(Protocol::OBJECT_PLAYER);

	// objectId 상위 16비트가 타입이므로 타입을 정한 직후에 발급한다.
	SetObjId(ObjectIdGenerator::GenerateObjectId(GetObjType()));

	// 길찾기 NavGrid 번들링 풋프린트(타일 단위). 클라 예측과 공유하는 상수(Shared/MovementMath.h).
	SetFootprint(MoveMath::PLAYER_FOOTPRINT_TILES_WIDE, MoveMath::PLAYER_FOOTPRINT_TILES_HIGH);

	// 벽 충돌 박스(셀 단위, 위치 중심). 스프라이트 8x8 전체를 덮는다. 클라 예측과 공유.
	SetCollisionBox(MoveMath::PLAYER_COLLISION_CELLS_WIDE, MoveMath::PLAYER_COLLISION_CELLS_HIGH);

	// 원형 충돌 반경(투사체 명중 / 쿼드트리) + 체력. // TODO PlayerData 테이블로 이관.
	SetRadius(2);
	SetMaxHp(100);
	SetHp(100);
}

void Player::FillObjectInfo(Protocol::ObjectInfo* info)
{
	GameObject::FillObjectInfo(info);

	Protocol::PlayerInfo* playerInfo = info->mutable_player();
	playerInfo->set_objectid(GetObjId());
	playerInfo->set_name(_name);
}
