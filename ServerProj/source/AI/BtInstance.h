#pragma once
#include "AI/BehaviorTree.h"

/*--------------
	BtInstance

	몬스터 한 마리가 드는 것. 이게 개체별 상태의 전부다.

	트리는 공유이므로 여기서는 참조만 든다(소유하지 않는다).
	진행 상태는 컴포짓 수만큼의 int16 배열 하나뿐이다.
	노드 수가 아니라 컴포짓 수인 이유는 재개 지점이 필요한 노드가 컴포짓뿐이기 때문이다.

	주의 - 룸 잡 큐 안에서만 쓴다. 스레드 안전하지 않다.
---------------*/

class BtInstance
{
public:
	// 트리가 바뀌면 상태를 전부 초기화한다.
	void		Bind(const BehaviorTree* tree);
	bool		IsBound() const { return _tree != nullptr; }

	const BehaviorTree*	GetTree() const { return _tree; }

	BtStatus	Tick(BtContext& context);
	void		Reset();

	Blackboard&			GetBlackboard()			{ return _blackboard; }
	const Blackboard&	GetBlackboard() const	{ return _blackboard; }

	/* Debug */
	BtStatus		GetLastStatus() const	{ return _lastStatus; }
	int64			GetTickCount() const	{ return _tickCount; }
	std::wstring	DescribeState() const;
	std::wstring	DescribeLastVisit() const;

private:
	BtStatus	Run(int32 nodeIndex, BtContext& context);

private:
	const BehaviorTree*	_tree = nullptr;	// 공유. 소유하지 않는다

	Blackboard		_blackboard;
	Vector<int16>	_compositeState;	// 컴포짓별 재개할 자식 번호

	BtStatus		_lastStatus = BtStatus::Failure;
	int64			_tickCount = 0;

	Vector<int32>	_lastVisited;		// Debug : 마지막 틱에 방문한 노드 순서
};
