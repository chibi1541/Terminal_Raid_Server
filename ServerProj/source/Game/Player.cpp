#include "pch.h"
#include "Game/Player.h"
#include "Game/CharacterData.h"
#include "Game/ObjectIdGenerator.h"

Player::Player()
{
	SetObjType(Protocol::OBJECT_PLAYER);

	// objectId 상위 16비트가 타입이므로 타입을 정한 직후에 발급한다.
	SetObjId(ObjectIdGenerator::GenerateObjectId(GetObjType()));

	// 벽 충돌 박스(셀 단위, 위치 중심). ★ 이 값은 클라 예측(MoveMath::PLAYER_COLLISION_CELLS_*)과
	// 반드시 같아야 한다. CharacterData 의 collisionCells 도 이 값으로 맞춰 둔다.
	SetCollisionBox(MoveMath::PLAYER_COLLISION_CELLS_WIDE, MoveMath::PLAYER_COLLISION_CELLS_HIGH);

	// 기본 캐릭터로 스탯을 채운다 (아직 클라 캐릭터 선택이 없어 서버가 정한다).
	SetCharacterType(CharacterData::Get().GetDefaultType());
}

void Player::SetCharacterType(Protocol::CharacterType type)
{
	_characterType = type;

	const CharacterDef& def = CharacterData::Get().Find(type);

	SetFootprint(def.footprintTiles, 1);	// 길찾기 NavGrid 번들링 (타일)
	SetRadius(def.radius);				// 원형 충돌 반경 (투사체 명중 / 쿼드트리)
	SetMaxHp(def.maxHp);
	SetHp(def.maxHp);
	SetAttackPower(def.attackPower);

	// collisionCells 가 PLAYER_COLLISION_CELLS_* 와 같을 때만 클라 예측과 안 어긋난다.
	if (def.collisionCells > 0)
		SetCollisionBox(def.collisionCells, def.collisionCells);

	// ★ moveSpeed 는 설정하지 않는다 ★ - 클라 ReplayInputs 가 MoveMath::DEFAULT_MOVE_SPEED_SUBUNITS
	// 를 하드코딩해 쓰므로, 서버가 다른 값을 넣으면 예측/재조정이 깨진다.
}

void Player::FillObjectInfo(Protocol::ObjectInfo* info)
{
	GameObject::FillObjectInfo(info);

	Protocol::PlayerInfo* playerInfo = info->mutable_player();
	playerInfo->set_objectid(GetObjId());
	playerInfo->set_name(_name);
	playerInfo->set_chartype(_characterType);
}
