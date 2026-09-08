#pragma once

#include "Protocol/Enum.pb.h"

#include <string>
#include <unordered_map>

/*-----------
	MonsterData

	몬스터 정의 테이블. 서버-클라 공통 (Server/Data/MonsterData.xml,
	sync_actor_data.py 가 클라로 복제 - 클라는 ActorDataAsset 이 읽는다).

	id 는 Enum.proto 의 MonsterType (문자열 -> MonsterType_Parse). 서버 부팅 시 1회 로드,
	실패해도 컴파일 폴백으로 뜬다.
------------*/

struct MonsterDef
{
	Protocol::MonsterType	type = Protocol::Monster_None;

	std::string	animClip = "Zombie";	// 클라 재생용 (AnimationData.xml 키).
	std::string	aiTree = "monster_basic";	// Data/AI/<이름>.canvas. 스폰 시 자동 부착. 빈 문자열이면 AI 없음.

	int32	collisionCells = 8;		// 벽 충돌 박스 한 변 (셀, 위치 중심). 이동 충돌 + 길찾기 팽창 공용.
	int32	radius = 4;				// 원형 충돌 반경 (셀). 투사체 명중 / 쿼드트리.
	int32	maxHp = 60;
	int32	attackPower = 8;
	int32	moveSpeedCells = 8;		// 이동 속도 (셀/초).

	int32	hitStunMs = 0;			// 피격 경직 시간(ms). 0 = 경직 면역(보스). 클라 Hit 클립 길이에 맞춘다.
	int32	deathFadeMs = 0;		// 사망 후 디스폰까지 지연(ms) - Death 클립 재생 시간. 0 = 즉시 디스폰.
};

class MonsterData
{
public:
	static MonsterData& Get();

	bool LoadFromFile(const WCHAR* path);

	Protocol::MonsterType	GetDefaultType() const { return _defaultType; }
	const MonsterDef&		Find(Protocol::MonsterType type) const;
	const MonsterDef&		GetDefault() const { return Find(_defaultType); }

private:
	std::unordered_map<int, MonsterDef>	_defs;	// key = (int)MonsterType
	Protocol::MonsterType				_defaultType = Protocol::Monster_Zombie;
	MonsterDef							_fallback;
};
