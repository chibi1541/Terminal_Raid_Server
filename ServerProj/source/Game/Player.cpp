#include "pch.h"
#include "Game/Player.h"
#include "Game/ObjectIdGenerator.h"

Player::Player()
{
	SetObjType(Protocol::OBJECT_PLAYER);

	// objectId 상위 16비트가 타입이므로 타입을 정한 직후에 발급한다.
	SetObjId(ObjectIdGenerator::GenerateObjectId(GetObjType()));

	// 길찾기 풋프린트 : 가로 2 x 세로 1 타일.
	SetFootprint(2, 1);
}

void Player::FillObjectInfo(Protocol::ObjectInfo* info)
{
	GameObject::FillObjectInfo(info);

	Protocol::PlayerInfo* playerInfo = info->mutable_player();
	playerInfo->set_objectid(GetObjId());
	playerInfo->set_name(_name);
}
