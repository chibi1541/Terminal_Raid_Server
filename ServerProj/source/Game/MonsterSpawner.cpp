#include "pch.h"
#include "MonsterSpawner.h"
#include "Room.h"
#include "Game/Monster.h"
#include "Game/MonsterData.h"
#include "Game/Level.h"

namespace
{
	// 좌표는 전부 셀 단위. 레벨(Server/Data/Level01.xml) 380 x 280, 프롭 배치
	// (Client/Assets/Level/Cemetery/Cemetery.LevelLayout.xml) 와 맞춘 값이다.
	constexpr int32 MAP_W = 380;
	constexpr int32 MAP_H = 280;

	// 맵 정중앙 = 십자로 교차점. 보스가 여기서 젠된다.
	constexpr int32 BOSS_CELL_X = MAP_W / 2;	// 190
	constexpr int32 BOSS_CELL_Y = MAP_H / 2;	// 140

	// 왼쪽 게이트 : 서벽 개구부(콜리전 맵 기준 안쪽 가장자리 x=18, 세로로 y 145~147).
	// 그 앞(맵 안쪽 = +x)으로 5 칸 떨어진 곳을 플레이어 최초 스폰으로 쓴다.
	constexpr int32 WEST_GATE_INNER_X = 18;
	constexpr int32 WEST_GATE_Y       = 146;
	constexpr int32 PLAYER_START_X    = WEST_GATE_INNER_X + 5;	// 23
	constexpr int32 PLAYER_START_Y    = WEST_GATE_Y;				// 146

	struct QuadRect
	{
		int32 minX, minY, maxX, maxY;
	};

	// 묘지 사분면. 십자로(세로 x 178~201, 가로 y 128~151)가 내부를 넷으로 가른다.
	// 각 사분면의 묘비 클러스터를 감싸는 사각형 - 이 안의 통행 가능 셀에 좀비를 뿌린다.
	constexpr QuadRect QUADS[4] =
	{
		{  36,  24, 170, 122 },	// 북서
		{ 214,  24, 344, 122 },	// 북동
		{  36, 168, 170, 264 },	// 남서
		{ 214, 168, 344, 264 },	// 남동
	};

	constexpr int32 ZOMBIES_PER_QUAD_MIN = 3;
	constexpr int32 ZOMBIES_PER_QUAD_MAX = 4;

	// 리젠 활성 상태에서 이 간격마다 1 마리씩 보충한다.
	constexpr float REGEN_INTERVAL_SEC = 4.0f;

	constexpr int32 FIND_CELL_MAX_TRY = 64;
}

void MonsterSpawner::SpawnInitial()
{
	if (_room == nullptr)
		return;

	SpawnBoss();

	const Level& level = _room->GetLevel();

	for (const QuadRect& quad : QUADS)
	{
		const int32 count = RandomRange32(ZOMBIES_PER_QUAD_MIN, ZOMBIES_PER_QUAD_MAX);

		for (int32 i = 0; i < count; i++)
		{
			int32 x = 0;
			int32 y = 0;
			bool found = false;

			for (int32 t = 0; t < FIND_CELL_MAX_TRY; t++)
			{
				x = RandomRange32(quad.minX, quad.maxX);
				y = RandomRange32(quad.minY, quad.maxY);
				if (level.IsCellBlocked(x, y) == false)
				{
					found = true;
					break;
				}
			}

			if (found == false)
				continue;

			SpawnZombieAt(x, y);
		}
	}

	_zombieTarget = static_cast<int32>(_zombieIds.size());

	LOG_INFO(L"[spawner] initial : boss=1 zombies=%d (target=%d)",
		static_cast<int32>(_zombieIds.size()), _zombieTarget);
}

void MonsterSpawner::Tick(float deltaTime)
{
	if (_room == nullptr || _zombieTarget <= 0)
		return;

	const int32 alive = CountAliveZombies();

	// 살아있는 좀비가 초기 총수의 절반 이하로 줄면 리젠을 켠다.
	if (_regenActive == false && alive * 2 <= _zombieTarget)
	{
		_regenActive = true;
		_regenTimer  = REGEN_INTERVAL_SEC;
		LOG_INFO(L"[spawner] regen ON (alive=%d / target=%d)", alive, _zombieTarget);
	}

	if (_regenActive == false)
		return;

	// 목표치를 회복하면 리젠을 끈다.
	if (alive >= _zombieTarget)
	{
		_regenActive = false;
		LOG_INFO(L"[spawner] regen OFF (alive=%d / target=%d)", alive, _zombieTarget);
		return;
	}

	_regenTimer -= deltaTime;
	if (_regenTimer > 0.0f)
		return;

	_regenTimer = REGEN_INTERVAL_SEC;

	// 무작위 사분면의 통행 가능 셀에 한 마리.
	const QuadRect& quad = QUADS[RandomRange32(0, 3)];
	const Level& level = _room->GetLevel();

	for (int32 t = 0; t < FIND_CELL_MAX_TRY; t++)
	{
		const int32 x = RandomRange32(quad.minX, quad.maxX);
		const int32 y = RandomRange32(quad.minY, quad.maxY);
		if (level.IsCellBlocked(x, y))
			continue;

		SpawnZombieAt(x, y);
		break;
	}
}

Protocol::Vector2 MonsterSpawner::GetPlayerStartPos() const
{
	Protocol::Vector2 pos;
	pos.set_x(PLAYER_START_X);
	pos.set_y(PLAYER_START_Y);
	return pos;
}

uint64 MonsterSpawner::SpawnZombieAt(int32 cellX, int32 cellY)
{
	MonsterRef monster = MakeShared<Monster>();
	monster->SetMonsterType(Protocol::Monster_Zombie);
	monster->SetPos(cellX, cellY);

	// 이미 룸 잡 큐 안이므로 DoAsync 없이 바로 부른다 (spawnmonster 명령과 같은 규약).
	_room->Enter(static_pointer_cast<GameObject>(monster), false);

	const string& aiTree = MonsterData::Get().Find(Protocol::Monster_Zombie).aiTree;
	if (aiTree.empty() == false)
		_room->AttachBehavior(monster->GetObjId(), aiTree);

	_zombieIds.push_back(monster->GetObjId());
	return monster->GetObjId();
}

void MonsterSpawner::SpawnBoss()
{
	MonsterRef boss = MakeShared<Monster>();
	boss->SetMonsterType(Protocol::Monster_Necromancer);
	boss->SetPos(BOSS_CELL_X, BOSS_CELL_Y);

	_room->Enter(static_pointer_cast<GameObject>(boss), false);

	const string& aiTree = MonsterData::Get().Find(Protocol::Monster_Necromancer).aiTree;
	if (aiTree.empty() == false)
		_room->AttachBehavior(boss->GetObjId(), aiTree);

	_bossId = boss->GetObjId();
}

int32 MonsterSpawner::CountAliveZombies()
{
	// 죽었거나(시체 페이드 중 IsAlive false) 이미 룸에서 빠진 id 를 걷어내며 제자리 압축한다.
	// (StlAllocator 에 operator== 이 없어 vector::swap 을 못 쓴다 - resize 로 줄인다)
	int32 alive = 0;
	int32 write = 0;

	for (int32 read = 0; read < static_cast<int32>(_zombieIds.size()); read++)
	{
		const uint64 id = _zombieIds[read];

		GameObjectRef obj = _room->Find(id);
		if (obj == nullptr || obj->IsAlive() == false)
			continue;

		_zombieIds[write++] = id;
		alive++;
	}

	_zombieIds.resize(write);
	return alive;
}
