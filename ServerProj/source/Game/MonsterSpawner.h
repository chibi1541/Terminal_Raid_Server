#pragma once
#include "Protocol/Struct.pb.h"
#include "Protocol/Enum.pb.h"

class Room;

/*-----------
	MonsterSpawner

	레벨에 몬스터를 채우고, 개체 수가 줄면 다시 채운다.

	배치 정보는 XML 이 아니라 이 클래스가 하드하게 들고 있다 (레벨이 하나뿐이라 데이터화 이득이 없다).
	  - 보스(Necromancer) 1 마리 : 맵 정중앙(십자로 교차점).
	  - 좀비 무리 : 묘지 네 사분면에 각 3~4 마리.
	  - 살아있는 좀비가 초기 총수의 절반 이하로 줄면 리젠을 켜고, 인터벌마다 1 마리씩 목표치까지 보충한다.
	  - 플레이어 최초 스폰 좌표(왼쪽 게이트 앞)도 여기서 준다. Room::Enter 가 참조한다.

	전부 룸 잡 큐 안에서만 부를 것. Room 이 소유하고 BeginPlay / Tick 에서 부른다.
------------*/

class MonsterSpawner
{
public:
	void	SetRoom(Room* room)	{ _room = room; }

	// 레벨 배치대로 보스 + 사분면 좀비 무리를 스폰. Room::BeginPlay 에서 1회.
	void	SpawnInitial();

	// 매 틱. 살아있는 좀비 수를 세고 절반 이하로 줄었으면 인터벌마다 1 마리씩 보충한다.
	void	Tick(float deltaTime);

	// 플레이어가 처음 입장할 때 놓일 셀. 왼쪽 게이트 안쪽에서 +x 로 조금 떨어진 곳.
	Protocol::Vector2	GetPlayerStartPos() const;

private:
	// 사분면 안에서 좀비 충돌 박스가 벽에 안 걸리는 무작위 셀을 고른다. 못 찾으면 false.
	bool	PickZombieCell(int32 quadIndex, int32& outX, int32& outY);

	// 좀비 하나를 (cellX, cellY) 에 스폰하고 AI 를 붙인다. 실패 시 0.
	uint64	SpawnZombieAt(int32 cellX, int32 cellY);

	// 보스 하나를 맵 중앙에 스폰하고 AI 를 붙인다.
	void	SpawnBoss();

	// _zombieIds 에서 죽은/사라진 id 를 걷어내고 살아있는 수를 돌려준다.
	int32	CountAliveZombies();

private:
	Room*			_room = nullptr;

	Vector<uint64>	_zombieIds;			// 추적 중인 좀비. CountAliveZombies 가 정리한다.
	uint64			_bossId = 0;

	int32			_zombieTarget = 0;	// 초기 총 좀비 수 = 리젠 목표치.
	float			_regenTimer = 0.0f;	// 리젠 인터벌 카운트다운 (활성일 때만 감소).
	bool			_regenActive = false;
};
