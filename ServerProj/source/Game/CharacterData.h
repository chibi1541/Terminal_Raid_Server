#pragma once

#include "Protocol/Enum.pb.h"

#include <string>
#include <unordered_map>

/*-----------
	CharacterData

	플레이어 캐릭터 정의 테이블. 서버-클라 공통 (Server/Data/CharacterData.xml,
	sync_actor_data.py 가 클라로 복제 - 클라는 ActorDataAsset 이 읽는다).

	id 는 Enum.proto 의 CharacterType (문자열 -> CharacterType_Parse). MonsterData 와 같은 규약.
	서버는 collision/stat 만, 클라는 animClip 도 함께 쓴다.
------------*/

struct CharacterDef
{
	Protocol::CharacterType	type = Protocol::CHARACTER_NONE;

	std::string	animClip = "Knight";	// 서버는 안 쓴다 (클라 재생용). 로그/디버그에만.

	int32	footprintTiles = 2;		// 길찾기 NavGrid 번들링 (타일).
	int32	collisionCells = 8;		// 벽 충돌 박스 한 변 (셀, 위치 중심). 스프라이트 8x8.
	int32	radius = 2;				// 원형 충돌 반경 (셀). 투사체 명중 / 쿼드트리.
	int32	maxHp = 100;
	int32	attackPower = 10;
	int32	moveSpeedCells = 20;	// 이동 속도 (셀/초). ★ 클라 예측(MoveMath)과 아직 분리 - 스폰 리팩터 때 통합.
	int32	hitStunMs = 300;		// 피격 경직 시간(ms). 이 동안 이동 입력 무시. 클라 Hit 클립 길이에 맞춘다.
};

class CharacterData
{
public:
	static CharacterData& Get();

	bool LoadFromFile(const WCHAR* path);

	Protocol::CharacterType	GetDefaultType() const { return _defaultType; }
	const CharacterDef&		Find(Protocol::CharacterType type) const;
	const CharacterDef&		GetDefault() const { return Find(_defaultType); }

private:
	std::unordered_map<int, CharacterDef>	_defs;	// key = (int)CharacterType
	Protocol::CharacterType					_defaultType = Protocol::CHARACTER_KNIGHT;
	CharacterDef							_fallback;
};
