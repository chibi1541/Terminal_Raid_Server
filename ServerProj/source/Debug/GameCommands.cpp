#include "pch.h"
#include "Debug/GameCommands.h"
#include "CommandRegistry.h"
#include "Room.h"
#include "Game/Player.h"
#include "Game/NavGrid.h"

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

	GCommandRegistry->Register(L"spawn", L"spawn <name> [x] [y]",
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
				context.Reply(L"usage : spawn <name> [x] [y]");
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

			// 이미 룸 잡 큐 안이므로 DoAsync 없이 바로 부른다.
			GRoom->Enter(static_pointer_cast<GameObject>(player), useRandomSpawnPos);

			context.Reply(L"spawned objectId=%llu at (%d, %d)",
				player->GetObjId(), player->GetPosX(), player->GetPosY());
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

			// TODO : 이동 패킷이 생기면 여기서 브로드캐스트.
			context.Reply(L"objectId=%llu moved to (%d, %d) (clients not notified yet)",
				objectId, x, y);
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
}
