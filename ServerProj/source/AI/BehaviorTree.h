#pragma once
#include "AI/BtLeaf.h"

/*----------------
	BehaviorTree

	공유 트리. 로드가 끝나면 완전히 불변이고, 몬스터 전원이 이 하나를 가리킨다.
	개체별 상태는 여기 없다. BtInstance 를 볼 것.

	노드는 평탄한 배열이고 자식은 연속 배치된다(firstChild 부터 childCount 개).
	부모가 자식보다 항상 앞에 온다.
-----------------*/

class BehaviorTree
{
public:
	struct Node
	{
		BtNodeType	type = BtNodeType::Leaf;
		int32		firstChild = -1;	// _nodes 인덱스. 자식이 없으면 -1
		int32		childCount = 0;
		int32		parent = -1;
		int32		leafIndex = -1;		// _leaves 인덱스. 리프가 아니면 -1
		int32		stateSlot = -1;		// 컴포짓 전용. BtInstance 진행 상태 배열의 인덱스
		string		label;				// 캔버스에 적힌 사람용 라벨. Debug 출력에만 쓴다
	};

public:
	BehaviorTree() = default;
	~BehaviorTree();

	BehaviorTree(const BehaviorTree&) = delete;
	BehaviorTree& operator=(const BehaviorTree&) = delete;

	int32			GetNodeCount() const		{ return static_cast<int32>(_nodes.size()); }
	int32			GetCompositeCount() const	{ return _compositeCount; }
	int32			GetRootIndex() const		{ return _rootIndex; }
	bool			IsValid() const				{ return _rootIndex >= 0; }

	const Node&		GetNode(int32 index) const	{ return _nodes[index]; }
	const BtLeaf*	GetLeaf(int32 index) const	{ return _leaves[index]; }

	// 블랙보드 키 이름 -> 슬롯. 로드 시점에만 부른다. 없으면 -1.
	int32			FindBlackboardSlot(const string& key) const;
	const Vector<BbValue>&	GetBlackboardDefaults() const { return _blackboardDefaults; }

	const string&	GetName() const { return _name; }

	/* Debug : 들여쓰기 트리 + 해석된 자식 순서 + 블랙보드 선언 */
	std::wstring	Describe() const;

private:
	void			DescribeNode(int32 index, int32 depth, OUT std::wstring& result) const;

private:
	// 트리를 채우는 것은 로더뿐이다.
	friend class BtCanvasLoader;

	void			Clear();

private:
	string					_name;
	Vector<Node>			_nodes;
	Vector<BtLeaf*>			_leaves;			// 트리가 소유한다
	Vector<BbValue>			_blackboardDefaults;
	HashMap<string, int32>	_blackboardSlots;
	Vector<string>			_blackboardNames;	// Debug 출력용. 슬롯 순서와 같다
	int32					_rootIndex = -1;
	int32					_compositeCount = 0;
};
