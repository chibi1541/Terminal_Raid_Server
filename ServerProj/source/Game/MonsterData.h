#pragma once

#include <string>
#include <unordered_map>

/*-----------
	MonsterData

	몬스터 정의 테이블. 서버 전용 (Server/Data/MonsterData.xml).
	ProjectileData 와 같은 규약 - 서버 부팅 시 1회 로드, 실패해도 컴파일 폴백으로 뜬다.

	이름 문자열이 키다 (Monster::SetMonsterTypeName 이 넘기는 값).
	클라는 이 테이블을 안 읽는다 - 반경/HP 는 CreatureState 스냅샷으로 받는다.
------------*/

struct MonsterDef
{
	std::string	id;

	int32	footprintTiles = 2;		// 길찾기 NavGrid 번들링 (타일). 세로는 1 고정.
	int32	collisionCells = 8;		// 벽 충돌 박스 한 변 (셀, 위치 중심).
	int32	radius = 4;				// 원형 충돌 반경 (셀). 투사체 명중 / 쿼드트리.
	int32	maxHp = 50;
};

class MonsterData
{
public:
	static MonsterData& Get();

	bool LoadFromFile(const WCHAR* path);

	// 모르는 이름이면 폴백(goblin 급 기본값)을 돌려준다.
	const MonsterDef& Find(const std::string& id) const;

private:
	std::unordered_map<std::string, MonsterDef>	_defs;
	MonsterDef									_fallback;
};
