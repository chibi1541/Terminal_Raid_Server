#include "pch.h"
#include "Debug/GameCommands.h"
#include "CommandRegistry.h"
#include "Room.h"
#include "Game/Player.h"

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

	GCommandRegistry->Register(L"room", L"room", L"룸 상태와 개체 목록",
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
		L"세션 없는 더미 플레이어를 룸에 넣는다 (접속 중인 클라에 S_SPAWN이 간다)",
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
		L"룸에서 개체를 빼낸다 (접속 중인 클라에 S_DESPAWN이 간다)",
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
		L"개체 좌표를 강제로 옮긴다 (이동 동기화가 들어오면 브로드캐스트까지 붙일 자리)",
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
			context.Reply(L"objectId=%llu moved to (%d, %d) (클라에는 아직 안 알림)",
				objectId, x, y);
		}, CommandRunMode::GameThread);
}
