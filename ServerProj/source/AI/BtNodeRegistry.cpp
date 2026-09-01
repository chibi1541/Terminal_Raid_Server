#include "pch.h"
#include "AI/BtNodeRegistry.h"
#include "AI/BehaviorTree.h"
#include <algorithm>

namespace
{
	// 전역 정적 컨테이너를 그대로 두면 GMemory 초기화 순서에 걸린다.
	// (Room 을 main 에서 만드는 것과 같은 이유)
	// 함수 지역 static 은 첫 호출 시점에 만들어지므로 안전하다.
	std::map<string, BtLeafFactory>& GetTable()
	{
		static std::map<string, BtLeafFactory> table;
		return table;
	}
}

void BtNodeRegistry::Register(const char* name, BtLeafFactory factory)
{
	if (name == nullptr || factory == nullptr)
		return;

	GetTable()[name] = std::move(factory);
}

bool BtNodeRegistry::Contains(const string& name)
{
	return GetTable().find(name) != GetTable().end();
}

BtLeaf* BtNodeRegistry::Create(const string& name, const BtParams& params, const BehaviorTree& tree)
{
	auto& table = GetTable();
	auto findIt = table.find(name);

	if (findIt == table.end())
		return nullptr;

	return findIt->second(params, tree);
}

std::wstring BtNodeRegistry::DescribeAll()
{
	auto& table = GetTable();

	WCHAR buffer[128];
	::swprintf_s(buffer, L"registered leaves : %d", static_cast<int32>(table.size()));

	std::wstring result = buffer;

	for (auto& item : table)
	{
		::swprintf_s(buffer, L"\n  %hs", item.first.c_str());
		result += buffer;
	}

	return result;
}
