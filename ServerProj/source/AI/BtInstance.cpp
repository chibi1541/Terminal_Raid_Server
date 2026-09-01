#include "pch.h"
#include "AI/BtInstance.h"

void BtInstance::Bind(const BehaviorTree* tree)
{
	_tree = tree;
	Reset();
}

void BtInstance::Reset()
{
	_lastStatus = BtStatus::Failure;
	_tickCount = 0;
	_lastVisited.clear();

	_compositeState.clear();
	_blackboard.Init(Vector<BbValue>());

	if (_tree == nullptr)
		return;

	// 트리가 선언한 기본값을 복사해 온다. 여기서부터는 개체마다 따로 논다.
	_blackboard.Init(_tree->GetBlackboardDefaults());
	_compositeState.resize(_tree->GetCompositeCount(), 0);
}

BtStatus BtInstance::Tick(BtContext& context)
{
	if (_tree == nullptr || _tree->IsValid() == false)
		return BtStatus::Failure;

	context.blackboard = &_blackboard;

	_lastVisited.clear();
	_lastStatus = Run(_tree->GetRootIndex(), context);
	_tickCount++;

	return _lastStatus;
}

BtStatus BtInstance::Run(int32 nodeIndex, BtContext& context)
{
	const BehaviorTree::Node& node = _tree->GetNode(nodeIndex);

	_lastVisited.push_back(nodeIndex);

	switch (node.type)
	{
	case BtNodeType::Sequence:
	{
		// Running 이던 자식부터 재개한다. 앞의 자식을 다시 실행하지 않는다.
		int16& resume = _compositeState[node.stateSlot];

		for (int32 i = resume; i < node.childCount; i++)
		{
			const BtStatus status = Run(node.firstChild + i, context);

			if (status == BtStatus::Running)
			{
				resume = static_cast<int16>(i);
				return BtStatus::Running;
			}

			if (status == BtStatus::Failure)
			{
				resume = 0;
				return BtStatus::Failure;
			}
		}

		resume = 0;
		return BtStatus::Success;
	}

	case BtNodeType::Selector:
	{
		int16& resume = _compositeState[node.stateSlot];

		for (int32 i = resume; i < node.childCount; i++)
		{
			const BtStatus status = Run(node.firstChild + i, context);

			if (status == BtStatus::Running)
			{
				resume = static_cast<int16>(i);
				return BtStatus::Running;
			}

			if (status == BtStatus::Success)
			{
				resume = 0;
				return BtStatus::Success;
			}
		}

		resume = 0;
		return BtStatus::Failure;
	}

	case BtNodeType::Inverter:
	{
		const BtStatus status = Run(node.firstChild, context);

		if (status == BtStatus::Running)
			return BtStatus::Running;

		return (status == BtStatus::Success) ? BtStatus::Failure : BtStatus::Success;
	}

	case BtNodeType::Succeeder:
	{
		const BtStatus status = Run(node.firstChild, context);

		if (status == BtStatus::Running)
			return BtStatus::Running;

		return BtStatus::Success;
	}

	case BtNodeType::Leaf:
	{
		const BtLeaf* leaf = _tree->GetLeaf(node.leafIndex);

		if (leaf == nullptr)
			return BtStatus::Failure;

		// 리프는 공유물이라 Execute 가 const 다. 상태는 전부 context 를 통해 오간다.
		return leaf->Execute(context);
	}
	}

	return BtStatus::Failure;
}

/*-------------
	Debug 출력
--------------*/

std::wstring BtInstance::DescribeState() const
{
	if (_tree == nullptr)
		return L"not bound";

	WCHAR buffer[256];
	::swprintf_s(buffer, L"tree %hs, ticks %lld, last %s",
		_tree->GetName().c_str(), _tickCount, ToString(_lastStatus));

	std::wstring result = buffer;

	result += L"\n  blackboard :";

	for (int32 slot = 0; slot < _blackboard.GetSlotCount(); slot++)
	{
		::swprintf_s(buffer, L"\n    [%d] = %s", slot, _blackboard.Get(slot).Describe().c_str());
		result += buffer;
	}

	result += L"\n  composite resume :";

	for (size_t i = 0; i < _compositeState.size(); i++)
	{
		::swprintf_s(buffer, L"\n    slot %d -> child %d",
			static_cast<int32>(i), static_cast<int32>(_compositeState[i]));

		result += buffer;
	}

	return result;
}

std::wstring BtInstance::DescribeLastVisit() const
{
	if (_lastVisited.empty())
		return L"(no visit)";

	std::wstring result;
	WCHAR buffer[64];

	for (size_t i = 0; i < _lastVisited.size(); i++)
	{
		::swprintf_s(buffer, L"%s%d", (i == 0) ? L"" : L" -> ", _lastVisited[i]);
		result += buffer;
	}

	return result;
}
