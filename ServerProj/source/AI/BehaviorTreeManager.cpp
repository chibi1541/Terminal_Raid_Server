#include "pch.h"
#include "AI/BehaviorTreeManager.h"
#include "AI/BtCanvasLoader.h"

namespace
{
	// Server.exe 는 Binaries 에서 실행되므로 프로젝트 루트까지 한 단계 올라간다.
	// (Config/Level01.xml 과 같은 기준)
	std::wstring MakePath(const string& name)
	{
		std::wstring wideName(name.begin(), name.end());
		return L"../Config/AI/" + wideName + L".canvas";
	}
}

const BehaviorTree* BehaviorTreeManager::Find(const string& name) const
{
	auto findIt = _trees.find(name);
	if (findIt == _trees.end())
		return nullptr;

	return findIt->second.get();
}

const BehaviorTree* BehaviorTreeManager::Load(const string& name)
{
	if (const BehaviorTree* cached = Find(name))
		return cached;

	std::unique_ptr<BehaviorTree> tree = std::make_unique<BehaviorTree>();

	if (BtCanvasLoader::LoadFromFile(MakePath(name).c_str(), name, OUT *tree) == false)
		return nullptr;

	BehaviorTree* raw = tree.get();
	_trees[name] = std::move(tree);

	return raw;
}

const BehaviorTree* BehaviorTreeManager::Reload(const string& name)
{
	auto findIt = _trees.find(name);

	if (findIt == _trees.end())
		return Load(name);

	// 새 트리를 따로 만들어 성공했을 때만 갈아끼운다.
	// 바로 덮어쓰면 캔버스에 오타가 났을 때 멀쩡하던 트리까지 잃는다.
	std::unique_ptr<BehaviorTree> fresh = std::make_unique<BehaviorTree>();

	if (BtCanvasLoader::LoadFromFile(MakePath(name).c_str(), name, OUT *fresh) == false)
		return nullptr;

	BehaviorTree* raw = fresh.get();
	findIt->second = std::move(fresh);

	return raw;
}

std::wstring BehaviorTreeManager::DescribeAll() const
{
	WCHAR buffer[256];
	::swprintf_s(buffer, L"loaded trees : %d", static_cast<int32>(_trees.size()));

	std::wstring result = buffer;

	for (auto& item : _trees)
	{
		::swprintf_s(buffer, L"\n  %hs : %d nodes, %d composites",
			item.first.c_str(), item.second->GetNodeCount(), item.second->GetCompositeCount());

		result += buffer;
	}

	return result;
}
