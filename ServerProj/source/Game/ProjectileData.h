#pragma once

#include "Protocol/Enum.pb.h"

#include <string>
#include <unordered_map>

/*-----------
	ProjectileData

	투사체 정의 테이블. 서버-클라 공통 (Server/Config/ProjectileData.xml,
	sync_projectile_data.py 가 클라로 복제).

	id 는 Enum.proto 의 ProjectileType (문자열 -> ProjectileType_Parse).
	서버 부팅 시 1회 로드. 실패해도 컴파일 상수 폴백으로 서버는 뜬다.
	클라는 ActorDataAsset 이 같은 XML 을 읽는다.
------------*/

struct ProjectileDef
{
	Protocol::ProjectileType	type = Protocol::Projectile_None;

	int32	speedCellsPerSec = 60;
	int32	rangeCells = 40;
	int32	radius = 1;				// 충돌 반경 (셀). 1 = 지름 3.

	int32	fireIntervalMs = 250;	// 최소 발사 간격 (서버 강제).
	int32	spawnForwardCells = 3;	// 발사 기준점(플레이어 몸통 중심)에서 조준 방향으로 이만큼 앞에서 스폰.
};

class ProjectileData
{
public:
	static ProjectileData& Get();

	bool LoadFromFile(const WCHAR* path);

	// 플레이어 기본 공격이 쏘는 투사체.
	Protocol::ProjectileType	GetDefaultType() const { return _defaultType; }
	const ProjectileDef&		Find(Protocol::ProjectileType type) const;
	const ProjectileDef&		GetDefault() const { return Find(_defaultType); }

private:
	std::unordered_map<int, ProjectileDef>	_defs;	// key = (int)ProjectileType
	Protocol::ProjectileType				_defaultType = Protocol::Projectile_Pellet;
	ProjectileDef							_fallback;
};
