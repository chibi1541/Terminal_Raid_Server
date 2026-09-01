#include "pch.h"
#include "AI/BehaviorTree.h"

BehaviorTree::~BehaviorTree()
{
	Clear();
}

void BehaviorTree::Clear()
{
	for (BtLeaf* leaf : _leaves)
		delete leaf;

	_leaves.clear();
	_nodes.clear();
	_blackboardDefaults.clear();
	_blackboardSlots.clear();
	_blackboardNames.clear();
	_rootIndex = -1;
	_compositeCount = 0;
}

int32 BehaviorTree::FindBlackboardSlot(const string& key) const
{
	auto findIt = _blackboardSlots.find(key);
	if (findIt == _blackboardSlots.end())
		return -1;

	return findIt->second;
}

/*-------------
	Debug 출력
--------------*/

void BehaviorTree::DescribeNode(int32 index, int32 depth, OUT std::wstring& result) const
{
	const Node& node = _nodes[index];

	WCHAR buffer[512];
	std::wstring indent;

	for (int32 i = 0; i < depth; i++)
		indent += L"  ";

	std::wstring detail;

	if (node.type == BtNodeType::Leaf && node.leafIndex >= 0)
		detail = _leaves[node.leafIndex]->Describe();

	// 라벨은 캔버스에 적힌 사람용 설명이다.
	::swprintf_s(buffer, L"\n  %s[%d] %s%s%hs%s%s",
		indent.c_str(), index, ToString(node.type),
		node.label.empty() ? L"" : L" ",
		node.label.c_str(),
		detail.empty() ? L"" : L"  ",
		detail.c_str());

	result += buffer;

	// 자식은 firstChild 부터 연속이고, 이 순서가 곧 실행 순서다.
	for (int32 i = 0; i < node.childCount; i++)
		DescribeNode(node.firstChild + i, depth + 1, OUT result);
}

std::wstring BehaviorTree::Describe() const
{
	WCHAR buffer[512];

	::swprintf_s(buffer, L"tree %hs : %d nodes, %d composites, %d blackboard slots",
		_name.c_str(), GetNodeCount(), _compositeCount,
		static_cast<int32>(_blackboardDefaults.size()));

	std::wstring result = buffer;

	if (_blackboardNames.empty() == false)
	{
		result += L"\n  blackboard :";

		for (size_t i = 0; i < _blackboardNames.size(); i++)
		{
			::swprintf_s(buffer, L"\n    [%d] %hs = %s",
				static_cast<int32>(i), _blackboardNames[i].c_str(),
				_blackboardDefaults[i].Describe().c_str());

			result += buffer;
		}
	}

	if (_rootIndex < 0)
	{
		result += L"\n  (empty tree)";
		return result;
	}

	result += L"\n  nodes (child order below is the resolved execution order) :";
	DescribeNode(_rootIndex, 0, OUT result);

	return result;
}
