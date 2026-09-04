#include "pch.h"
#include "Debug/GameCommands.h"
#include "CommandRegistry.h"
#include "Room.h"
#include "Protocol/ClientPacketHandler.h"
#include "Game/Player.h"
#include "Game/NavGrid.h"
#include "Game/QuadTree.h"
#include "AI/BehaviorTreeManager.h"
#include "AI/BtNodeRegistry.h"
#include "AI/BtInstance.h"
#include <algorithm>

namespace
{
	// 실패하면 false. 숫자가 아닌 인자를 조용히 0으로 먹지 않게 한다.
	bool ParseInt32(const std::wstring& text, OUT int32& outValue)
	{
		if (text.empty())
			return false;

		WCHAR* end = nullptr;
		const long value = ::wcstol(text.c_str(), &end, 10);

		if (end == nullptr || *end != L'\0')
			return false;

		outValue = static_cast<int32>(value);
		return true;
	}

	bool ParseUint64(const std::wstring& text, OUT uint64& outValue)
	{
		if (text.empty())
			return false;

		WCHAR* end = nullptr;
		const unsigned long long value = ::wcstoull(text.c_str(), &end, 10);

		if (end == nullptr || *end != L'\0')
			return false;

		outValue = static_cast<uint64>(value);
		return true;
	}

	// 8방향 키워드 -> DirectionType. 실패하면 false.
	bool ParseDir8(const std::wstring& text, OUT Protocol::DirectionType& out)
	{
		if      (text == L"stop" || text == L"none")	out = Protocol::DIR_NONE;
		else if (text == L"left")					out = Protocol::DIR_LEFT;
		else if (text == L"right")					out = Protocol::DIR_RIGHT;
		else if (text == L"up")						out = Protocol::DIR_UP;
		else if (text == L"down")					out = Protocol::DIR_DOWN;
		else if (text == L"ul")						out = Protocol::DIR_UP_LEFT;
		else if (text == L"ur")						out = Protocol::DIR_UP_RIGHT;
		else if (text == L"dl")						out = Protocol::DIR_DOWN_LEFT;
		else if (text == L"dr")						out = Protocol::DIR_DOWN_RIGHT;
		else return false;
		return true;
	}

	// 명령 인자는 wstring 이다. 트리 이름 같은 ASCII 값만 좁은 문자열로 옮긴다.
	string ToNarrow(const std::wstring& text)
	{
		string result;
		result.reserve(text.size());

		for (wchar_t ch : text)
			result += (ch < 128) ? static_cast<char>(ch) : '?';

		return result;
	}

	// 두 결과의 개체 집합이 같은지 본다. 순서는 다를 수 있으므로 id 로 정렬해 비교한다.
	bool SameObjectSet(Vector<GameObject*>& lhs, Vector<GameObject*>& rhs)
	{
		if (lhs.size() != rhs.size())
			return false;

		auto byId = [](GameObject* a, GameObject* b) { return a->GetObjId() < b->GetObjId(); };

		std::sort(lhs.begin(), lhs.end(), byId);
		std::sort(rhs.begin(), rhs.end(), byId);

		for (size_t i = 0; i < lhs.size(); i++)
		{
			if (lhs[i]->GetObjId() != rhs[i]->GetObjId())
				return false;
		}

		return true;
	}
}

void GameCommands::Register()
{
	if (GCommandRegistry == nullptr)
		return;

	GCommandRegistry->Register(L"room", L"room", L"room state and object list",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			context.Reply(L"%s", GRoom->DescribeObjects().c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"spawn", L"spawn <name> [x] [y] [radius]",
		L"add a session-less dummy player to the room (connected clients get S_SPAWN)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			if (context.ArgCount() < 2)
			{
				context.Reply(L"usage : spawn <name> [x] [y] [radius]");
				return;
			}

			PlayerRef player = MakeShared<Player>();

			// 명령 인자는 wstring이라 이름을 좁은 문자열로 옮긴다.
			// 디버그용이므로 ASCII가 아닌 글자는 '?'로 떨군다.
			const std::wstring& wideName = context.Arg(1);
			string narrowName;
			narrowName.reserve(wideName.size());
			for (wchar_t ch : wideName)
				narrowName += (ch < 128) ? static_cast<char>(ch) : '?';

			player->SetName(narrowName);

			bool useRandomSpawnPos = true;

			if (context.ArgCount() >= 4)
			{
				int32 x = 0;
				int32 y = 0;

				if (ParseInt32(context.Arg(2), OUT x) == false ||
					ParseInt32(context.Arg(3), OUT y) == false)
				{
					context.Reply(L"invalid coordinates : %s %s",
						context.Arg(2).c_str(), context.Arg(3).c_str());
					return;
				}

				player->SetPos(x, y);
				useRandomSpawnPos = false;
			}

			// 반지름을 주면 분할선에 걸치는 개체를 만들 수 있다. 쿼드트리 검증에 필요하다.
			if (context.ArgCount() >= 5)
			{
				int32 radius = 0;

				if (ParseInt32(context.Arg(4), OUT radius) == false)
				{
					context.Reply(L"invalid radius : %s", context.Arg(4).c_str());
					return;
				}

				player->SetRadius(radius);
			}

			// 이미 룸 잡 큐 안이므로 DoAsync 없이 바로 부른다.
			GRoom->Enter(static_pointer_cast<GameObject>(player), useRandomSpawnPos);

			context.Reply(L"spawned objectId=%llu at (%d, %d) r=%d",
				player->GetObjId(), player->GetPosX(), player->GetPosY(), player->GetRadius());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"despawn", L"despawn <objectId>",
		L"remove an object from the room (connected clients get S_DESPAWN)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			uint64 objectId = 0;
			if (context.ArgCount() < 2 || ParseUint64(context.Arg(1), OUT objectId) == false)
			{
				context.Reply(L"usage : despawn <objectId>");
				return;
			}

			GameObjectRef object = GRoom->Find(objectId);
			if (object == nullptr)
			{
				context.Reply(L"no such object : %llu", objectId);
				return;
			}

			GRoom->Leave(object);
			context.Reply(L"despawned objectId=%llu", objectId);
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"tp", L"tp <objectId> <x> <y>",
		L"force an object position (broadcast comes later with movement sync)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			uint64 objectId = 0;
			int32 x = 0;
			int32 y = 0;

			if (context.ArgCount() < 4 ||
				ParseUint64(context.Arg(1), OUT objectId) == false ||
				ParseInt32(context.Arg(2), OUT x) == false ||
				ParseInt32(context.Arg(3), OUT y) == false)
			{
				context.Reply(L"usage : tp <objectId> <x> <y>");
				return;
			}

			GameObjectRef object = GRoom->Find(objectId);
			if (object == nullptr)
			{
				context.Reply(L"no such object : %llu", objectId);
				return;
			}

			object->SetPos(x, y);

			// 텔레포트는 이동 상태를 끊는다. 진행 중이던 경로 추종도 취소.
			MovementComponent& m = object->Movement();
			m.ClearPath();
			m.dir = Protocol::DIR_NONE;
			m.state = MoveState::Idle;
			m.dirty = false;

			// 순간이동을 S_MOVE 한 건으로 알린다 (dir=DIR_NONE 이라 클라는 스냅).
			Protocol::S_MOVE movePkt;
			movePkt.set_servertick(static_cast<uint32>(GRoom->GetTickCount()));
			Protocol::MoveInfo* info = movePkt.add_moves();
			info->set_objectid(objectId);
			info->mutable_pos()->CopyFrom(object->GetPos());
			info->set_dir(Protocol::DIR_NONE);
			info->set_speed(m.EffectiveSpeed());
			info->set_servertick(static_cast<uint32>(GRoom->GetTickCount()));
			GRoom->Broadcast(ClientPacketHandler::MakeSendBuffer(movePkt), 0);

			context.Reply(L"objectId=%llu moved to (%d, %d)", objectId, x, y);
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"move",
		L"move <objectId> <stop|left|right|up|down|ul|ur|dl|dr>",
		L"drive an object with a held direction (server integrates it every tick)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			uint64 objectId = 0;

			if (context.ArgCount() < 3 || ParseUint64(context.Arg(1), OUT objectId) == false)
			{
				context.Reply(L"usage : move <objectId> <stop|left|right|up|down|ul|ur|dl|dr>");
				return;
			}

			const std::wstring dirText = context.Arg(2);
			Protocol::DirectionType dir = Protocol::DIR_NONE;

			if (ParseDir8(dirText, OUT dir) == false)
			{
				context.Reply(L"unknown direction : %s", dirText.c_str());
				return;
			}

			GameObjectRef object = GRoom->Find(objectId);
			if (object == nullptr)
			{
				context.Reply(L"no such object : %llu", objectId);
				return;
			}

			// 이미 룸 잡 큐 안이므로 DoAsync 없이 바로. inputSeq 0 = 디버그 호출.
			GRoom->HandleMove(object, 0, 0, static_cast<int32>(dir));
			context.Reply(L"objectId=%llu dir=%s from (%d, %d)",
				objectId, dirText.c_str(), object->GetPosX(), object->GetPosY());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"goto", L"goto <objectId> <x> <y>",
		L"path an object to a cell with JPS and follow it",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			uint64 objectId = 0;
			int32 x = 0;
			int32 y = 0;

			if (context.ArgCount() < 4 ||
				ParseUint64(context.Arg(1), OUT objectId) == false ||
				ParseInt32(context.Arg(2), OUT x) == false ||
				ParseInt32(context.Arg(3), OUT y) == false)
			{
				context.Reply(L"usage : goto <objectId> <x> <y>");
				return;
			}

			if (GRoom->OrderMoveTo(objectId, x, y))
				context.Reply(L"objectId=%llu heading to (%d, %d)", objectId, x, y);
			else
				context.Reply(L"no path for objectId=%llu to (%d, %d)", objectId, x, y);
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"worldtick", L"worldtick [count]",
		L"advance the movement loop manually (default 1). autotick still runs unless paused elsewhere",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 count = 1;

			if (context.ArgCount() >= 2 && ParseInt32(context.Arg(1), OUT count) == false)
			{
				context.Reply(L"invalid count : %s", context.Arg(1).c_str());
				return;
			}

			if (count <= 0 || count > 1000)
			{
				context.Reply(L"count must be in 1..1000");
				return;
			}

			GRoom->DebugStepMovement(count);
			context.Reply(L"stepped movement %d tick(s), tickCount=%llu",
				count, GRoom->GetTickCount());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"proj",
		L"proj <x> <y> <dir> [speedCellsPerSec] [lifetimeSec]",
		L"spawn a straight-line projectile (dir : left/right/up/down/ul/ur/dl/dr)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 x = 0;
			int32 y = 0;

			if (context.ArgCount() < 4 ||
				ParseInt32(context.Arg(1), OUT x) == false ||
				ParseInt32(context.Arg(2), OUT y) == false)
			{
				context.Reply(L"usage : proj <x> <y> <dir> [speedCellsPerSec] [lifetimeSec]");
				return;
			}

			Protocol::DirectionType dir = Protocol::DIR_NONE;

			if (ParseDir8(context.Arg(3), OUT dir) == false || dir == Protocol::DIR_NONE)
			{
				context.Reply(L"bad direction : %s (left/right/up/down/ul/ur/dl/dr)",
					context.Arg(3).c_str());
				return;
			}

			int32 speed = 0;	// 0 => 기본 속도

			if (context.ArgCount() >= 5 && ParseInt32(context.Arg(4), OUT speed) == false)
			{
				context.Reply(L"bad speed : %s", context.Arg(4).c_str());
				return;
			}

			int32 lifeSec = 0;

			if (context.ArgCount() >= 6 && ParseInt32(context.Arg(5), OUT lifeSec) == false)
			{
				context.Reply(L"bad lifetime : %s", context.Arg(5).c_str());
				return;
			}

			// 틱은 50ms 고정 = 초당 20틱. 0 이면 Room 이 기본 수명을 쓴다.
			const int32 lifeTicks = (lifeSec > 0) ? lifeSec * 20 : 0;

			GameObjectRef proj = GRoom->SpawnProjectile(x, y, dir, speed, lifeTicks);

			if (proj == nullptr)
			{
				context.Reply(L"failed to spawn projectile");
				return;
			}

			context.Reply(L"projectile objectId=%llu at (%d, %d) dir=%s speed=%d",
				proj->GetObjId(), x, y, context.Arg(3).c_str(),
				(speed > 0) ? speed : DEFAULT_MOVE_SPEED_CELLS);
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"level", L"level", L"level info and tile map",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			context.Reply(L"%s", GRoom->DescribeLevel().c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"levelreload", L"levelreload", L"reload the level XML",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			if (GRoom->LoadLevel())
				context.Reply(L"level reloaded.\n%s", GRoom->DescribeLevel().c_str());
			else
				context.Reply(L"level load failed, fell back to empty map. check the log");
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"path", L"path <startTileX> <startTileY> [objectId]",
		L"find a path from a tile to a player using JPS (first player if objectId omitted)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 startX = 0;
			int32 startY = 0;

			if (context.ArgCount() < 3 ||
				ParseInt32(context.Arg(1), OUT startX) == false ||
				ParseInt32(context.Arg(2), OUT startY) == false)
			{
				context.Reply(L"usage : path <startTileX> <startTileY> [objectId]");
				return;
			}

			uint64 targetId = 0;

			if (context.ArgCount() >= 4)
			{
				if (ParseUint64(context.Arg(3), OUT targetId) == false)
				{
					context.Reply(L"invalid objectId : %s", context.Arg(3).c_str());
					return;
				}
			}
			else
			{
				targetId = GRoom->FindFirstPlayerId();

				if (targetId == 0)
				{
					context.Reply(L"no player in the room. use spawn to place a dummy, or connect a client");
					return;
				}
			}

			const TilePos rawStart{ startX, startY };

			Vector<TilePos> path;
			TilePos usedStart;
			TilePos usedGoal;

			LARGE_INTEGER freq = {};
			LARGE_INTEGER begin = {};
			LARGE_INTEGER end = {};
			::QueryPerformanceFrequency(OUT &freq);
			::QueryPerformanceCounter(OUT &begin);

			const bool found = GRoom->FindPathToObject(rawStart, targetId, OUT path, OUT usedStart, OUT usedGoal);

			::QueryPerformanceCounter(OUT &end);

			const double elapsedUs = (freq.QuadPart > 0)
				? (static_cast<double>(end.QuadPart - begin.QuadPart) * 1000000.0 / freq.QuadPart)
				: 0.0;

			if (found == false)
			{
				context.Reply(L"path not found : start (%d, %d) -> objectId %llu   expanded %d nodes, %.1f us",
					rawStart.x, rawStart.y, targetId,
					GRoom->GetLastExpandedCount(), elapsedUs);
				return;
			}

			WCHAR buffer[256];
			::swprintf_s(buffer, L"path %d tiles, jump points %d taken / %d opened, expanded %d nodes, %.1f us",
				static_cast<int32>(path.size()), GRoom->GetLastPathJumpPointCount(),
				GRoom->GetLastOpenedCount(), GRoom->GetLastExpandedCount(), elapsedUs);

			std::wstring header = buffer;

			// 스냅이 일어났으면 반드시 알려준다.
			// 안 그러면 요청한 좌표와 결과가 달라 보여 알고리즘을 의심하게 된다.
			if (usedStart != rawStart)
			{
				::swprintf_s(buffer, L"\nstart (%d, %d) -> (%d, %d) snapped (requested tile is blocked)",
					rawStart.x, rawStart.y, usedStart.x, usedStart.y);
				header += buffer;
			}

			::swprintf_s(buffer, L"\ngoal  objectId %llu -> tile (%d, %d)",
				targetId, usedGoal.x, usedGoal.y);
			header += buffer;

			context.Reply(L"%s\n%s", header.c_str(),
				GRoom->DescribePath(path, usedStart, usedGoal).c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"tree", L"tree", L"collision quadtree structure",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			// 명령은 틱 밖에서 실행되므로 최신 상태를 보려면 먼저 다시 세워야 한다.
			GRoom->RebuildCollisionTree();

			context.Reply(L"%s", GRoom->DescribeTree().c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"query", L"query <x> <y> <radius>",
		L"circle query against the quadtree, cross-checked with brute force",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 x = 0;
			int32 y = 0;
			int32 radius = 0;

			if (context.ArgCount() < 4 ||
				ParseInt32(context.Arg(1), OUT x) == false ||
				ParseInt32(context.Arg(2), OUT y) == false ||
				ParseInt32(context.Arg(3), OUT radius) == false)
			{
				context.Reply(L"usage : query <x> <y> <radius>");
				return;
			}

			GRoom->RebuildCollisionTree();

			Vector<GameObject*> treeResult;
			Vector<GameObject*> bruteResult;

			LARGE_INTEGER freq = {};
			LARGE_INTEGER begin = {};
			LARGE_INTEGER end = {};
			::QueryPerformanceFrequency(OUT &freq);
			::QueryPerformanceCounter(OUT &begin);

			GRoom->QueryCircle(x, y, radius, OUT treeResult);

			::QueryPerformanceCounter(OUT &end);

			GRoom->QueryCircleBruteForce(x, y, radius, OUT bruteResult);

			const double elapsedUs = (freq.QuadPart > 0)
				? (static_cast<double>(end.QuadPart - begin.QuadPart) * 1000000.0 / freq.QuadPart)
				: 0.0;

			const bool match = SameObjectSet(treeResult, bruteResult);

			WCHAR buffer[256];
			::swprintf_s(buffer,
				L"query (%d, %d) r=%d : %d hits, visited %d / %d nodes, %.1f us   [%s]",
				x, y, radius, static_cast<int32>(treeResult.size()),
				GRoom->GetLastVisitedNodes(), GRoom->GetTreeNodeCount(), elapsedUs,
				match ? L"MATCH" : L"MISMATCH vs brute force");

			std::wstring result = buffer;

			if (match == false)
			{
				::swprintf_s(buffer, L"\n  brute force found %d", static_cast<int32>(bruteResult.size()));
				result += buffer;
			}

			for (GameObject* object : treeResult)
			{
				::swprintf_s(buffer, L"\n  id=%llu pos=(%d, %d) r=%d",
					object->GetObjId(), object->GetPosX(), object->GetPosY(), object->GetRadius());
				result += buffer;
			}

			context.Reply(L"%s", result.c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"querytest", L"querytest [count]",
		L"run random circle queries and compare every result against brute force",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 count = 1000;

			if (context.ArgCount() >= 2 && ParseInt32(context.Arg(1), OUT count) == false)
			{
				context.Reply(L"usage : querytest [count]");
				return;
			}

			if (count <= 0 || count > 100000)
			{
				context.Reply(L"count must be in 1..100000");
				return;
			}

			GRoom->RebuildCollisionTree();

			const int32 width = static_cast<int32>(GRoom->GetWidth());
			const int32 height = static_cast<int32>(GRoom->GetHeight());

			Vector<GameObject*> treeResult;
			Vector<GameObject*> bruteResult;

			int32 mismatch = 0;
			int32 totalHits = 0;
			std::wstring firstFailure;

			for (int32 i = 0; i < count; i++)
			{
				// 맵 밖으로 조금 삐져나가는 질의도 섞이도록 범위를 넉넉히 잡는다.
				const int32 x = RandomRange32(-4, width + 3);
				const int32 y = RandomRange32(-4, height + 3);
				const int32 radius = RandomRange32(0, 12);

				GRoom->QueryCircle(x, y, radius, OUT treeResult);
				GRoom->QueryCircleBruteForce(x, y, radius, OUT bruteResult);

				totalHits += static_cast<int32>(treeResult.size());

				if (SameObjectSet(treeResult, bruteResult))
					continue;

				mismatch++;

				if (firstFailure.empty())
				{
					WCHAR buffer[256];
					::swprintf_s(buffer,
						L"\n  first failure : query %d %d %d   tree=%d brute=%d",
						x, y, radius, static_cast<int32>(treeResult.size()),
						static_cast<int32>(bruteResult.size()));
					firstFailure = buffer;
				}
			}

			WCHAR buffer[256];
			::swprintf_s(buffer, L"querytest : %d queries, %d hits total, %d mismatches",
				count, totalHits, mismatch);

			std::wstring result = buffer;
			result += firstFailure;

			context.Reply(L"%s", result.c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"spawnmany", L"spawnmany <count> [radius] [x] [y]",
		L"spawn many dummies at once (random walkable spots, or scattered around x y)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			int32 count = 0;

			if (context.ArgCount() < 2 || ParseInt32(context.Arg(1), OUT count) == false)
			{
				context.Reply(L"usage : spawnmany <count> [radius] [x] [y]");
				return;
			}

			if (count <= 0 || count > 2000)
			{
				context.Reply(L"count must be in 1..2000");
				return;
			}

			int32 radius = 0;

			if (context.ArgCount() >= 3 && ParseInt32(context.Arg(2), OUT radius) == false)
			{
				context.Reply(L"invalid radius : %s", context.Arg(2).c_str());
				return;
			}

			// x y 를 주면 그 근처에 몰아서 만든다. 안 주면 맵 전체에 흩뿌린다.
			bool clustered = false;
			int32 centerX = 0;
			int32 centerY = 0;

			if (context.ArgCount() >= 5)
			{
				if (ParseInt32(context.Arg(3), OUT centerX) == false ||
					ParseInt32(context.Arg(4), OUT centerY) == false)
				{
					context.Reply(L"invalid coordinates : %s %s",
						context.Arg(3).c_str(), context.Arg(4).c_str());
					return;
				}

				clustered = true;
			}

			for (int32 i = 0; i < count; i++)
			{
				PlayerRef player = MakeShared<Player>();
				player->SetName("bulk");
				player->SetRadius(radius);

				if (clustered)
				{
					// 분할선에 걸치는 개체가 섞이도록 좁게 흩어놓는다.
					player->SetPos(centerX + RandomRange32(-4, 4), centerY + RandomRange32(-4, 4));
				}

				GRoom->Enter(static_pointer_cast<GameObject>(player), clustered == false);
			}

			GRoom->RebuildCollisionTree();

			context.Reply(L"spawned %d dummies (radius %d)%s\n%s",
				count, radius, clustered ? L" clustered" : L" scattered",
				GRoom->DescribeTree().c_str());
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"despawnall", L"despawnall",
		L"remove every dummy spawned by debug commands (connected clients are kept)",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			const int32 removed = GRoom->RemoveAllDummies();
			GRoom->RebuildCollisionTree();

			context.Reply(L"removed %d dummies", removed);
		}, CommandRunMode::GameThread);

	GCommandRegistry->Register(L"bt", L"bt <leaves|load|reload|dump|attach|detach|step|state> ...",
		L"behavior tree debugging",
		[](CommandContext& context)
		{
			if (GRoom == nullptr)
			{
				context.Reply(L"room not created");
				return;
			}

			const std::wstring& sub = context.Arg(1);

			if (sub.empty() || ::_wcsicmp(sub.c_str(), L"help") == 0)
			{
				context.Reply(L"bt auto <on|off>              automatic ticking from Room::Tick\n"
					L"bt leaves                     registered leaf types\n"
					L"bt load <name>                load Config/AI/<name>.canvas\n"
					L"bt reload <name>              reload and rebind attached instances\n"
					L"bt dump [name]                tree structure with resolved child order\n"
					L"bt attach <objectId> <name>   attach an instance to an object\n"
					L"bt detach <objectId>          remove the instance\n"
					L"bt step <objectId> [count]    tick manually, print status and visit path\n"
					L"bt state <objectId>           blackboard and composite resume state");
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"auto") == 0)
			{
				if (context.ArgCount() >= 3)
				{
					const bool enable = (::_wcsicmp(context.Arg(2).c_str(), L"on") == 0);
					GRoom->SetBehaviorAutoTick(enable);
				}

				context.Reply(L"auto tick is %s", GRoom->IsBehaviorAutoTick() ? L"on" : L"off");
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"leaves") == 0)
			{
				context.Reply(L"%s", BtNodeRegistry::DescribeAll().c_str());
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"load") == 0)
			{
				if (context.ArgCount() < 3)
				{
					context.Reply(L"usage : bt load <name>");
					return;
				}

				const string name = ToNarrow(context.Arg(2));
				const BehaviorTree* tree = GRoom->GetBtManager().Load(name);

				if (tree == nullptr)
				{
					context.Reply(L"load failed. check the log for the reason");
					return;
				}

				context.Reply(L"%s", tree->Describe().c_str());
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"reload") == 0)
			{
				if (context.ArgCount() < 3)
				{
					context.Reply(L"usage : bt reload <name>");
					return;
				}

				const string name = ToNarrow(context.Arg(2));

				if (GRoom->ReloadBehaviorTree(name) == false)
				{
					context.Reply(L"reload failed, previous tree kept. check the log");
					return;
				}

				const BehaviorTree* tree = GRoom->GetBtManager().Find(name);
				context.Reply(L"reloaded, attached instances rebound.\n%s", tree->Describe().c_str());
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"dump") == 0)
			{
				if (context.ArgCount() < 3)
				{
					context.Reply(L"%s", GRoom->GetBtManager().DescribeAll().c_str());
					return;
				}

				const BehaviorTree* tree = GRoom->GetBtManager().Find(ToNarrow(context.Arg(2)));

				if (tree == nullptr)
				{
					context.Reply(L"not loaded. use 'bt load' first");
					return;
				}

				context.Reply(L"%s", tree->Describe().c_str());
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"attach") == 0)
			{
				uint64 objectId = 0;

				if (context.ArgCount() < 4 || ParseUint64(context.Arg(2), OUT objectId) == false)
				{
					context.Reply(L"usage : bt attach <objectId> <name>");
					return;
				}

				if (GRoom->AttachBehavior(objectId, ToNarrow(context.Arg(3))) == false)
				{
					context.Reply(L"attach failed : no such object, or the tree did not load");
					return;
				}

				context.Reply(L"attached %hs to objectId=%llu",
					ToNarrow(context.Arg(3)).c_str(), objectId);
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"detach") == 0)
			{
				uint64 objectId = 0;

				if (context.ArgCount() < 3 || ParseUint64(context.Arg(2), OUT objectId) == false)
				{
					context.Reply(L"usage : bt detach <objectId>");
					return;
				}

				context.Reply(GRoom->DetachBehavior(objectId)
					? L"detached" : L"no instance on that object");
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"step") == 0)
			{
				uint64 objectId = 0;

				if (context.ArgCount() < 3 || ParseUint64(context.Arg(2), OUT objectId) == false)
				{
					context.Reply(L"usage : bt step <objectId> [count]");
					return;
				}

				int32 count = 1;

				if (context.ArgCount() >= 4 && ParseInt32(context.Arg(3), OUT count) == false)
				{
					context.Reply(L"invalid count : %s", context.Arg(3).c_str());
					return;
				}

				if (count <= 0 || count > 1000)
				{
					context.Reply(L"count must be in 1..1000");
					return;
				}

				BtInstance* instance = GRoom->FindBehavior(objectId);

				if (instance == nullptr)
				{
					context.Reply(L"no instance on objectId=%llu", objectId);
					return;
				}

				std::wstring result;
				WCHAR buffer[512];

				for (int32 i = 0; i < count; i++)
				{
					const BtStatus status = GRoom->TickBehavior(objectId, GRoom->GetTickDeltaTime());

					::swprintf_s(buffer, L"%stick %lld : %s   visit %s",
						(i == 0) ? L"" : L"\n", instance->GetTickCount(),
						ToString(status), instance->DescribeLastVisit().c_str());

					result += buffer;
				}

				context.Reply(L"%s", result.c_str());
				return;
			}

			if (::_wcsicmp(sub.c_str(), L"state") == 0)
			{
				uint64 objectId = 0;

				if (context.ArgCount() < 3 || ParseUint64(context.Arg(2), OUT objectId) == false)
				{
					context.Reply(L"usage : bt state <objectId>");
					return;
				}

				BtInstance* instance = GRoom->FindBehavior(objectId);

				if (instance == nullptr)
				{
					context.Reply(L"no instance on objectId=%llu", objectId);
					return;
				}

				context.Reply(L"objectId=%llu\n%s", objectId, instance->DescribeState().c_str());
				return;
			}

			context.Reply(L"unknown subcommand : %s   (try 'bt help')", sub.c_str());
		}, CommandRunMode::GameThread);
}
