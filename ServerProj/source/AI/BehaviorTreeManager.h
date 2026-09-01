#pragma once
#include "AI/BehaviorTree.h"
#include <memory>

/*----------------------
	BehaviorTreeManager

	이름 -> 공유 트리 캐시. 같은 캔버스를 여러 몬스터가 써도 한 번만 읽는다.
	Room 이 소유하고, 룸 잡 큐 안에서만 만진다.

	트리는 unique_ptr 로 들고 있고 밖으로는 const 포인터만 내보낸다.
	=> 인스턴스 쪽에서 트리를 건드릴 방법이 없다.
-----------------------*/

class BehaviorTreeManager
{
public:
	// 이미 있으면 그걸 돌려주고, 없으면 Config/AI/<name>.canvas 를 읽는다.
	const BehaviorTree*	Load(const string& name);
	const BehaviorTree*	Find(const string& name) const;

	// 캔버스를 고쳐가며 반복 검증할 때. 실패하면 기존 트리를 그대로 두고 nullptr.
	//
	// 주의 - 성공하면 트리 객체가 새로 만들어져 주소가 바뀐다.
	// 붙어 있던 BtInstance 는 반드시 다시 Bind 해야 한다. (Room::ReloadBehaviorTree 참고)
	const BehaviorTree*	Reload(const string& name);

	/* Debug */
	std::wstring		DescribeAll() const;

private:
	// unique_ptr 로 드는 이유 - 트리 주소가 고정되어야 한다.
	// 값으로 담으면 맵이 재해싱될 때 주소가 바뀌어 BtInstance 의 참조가 끊긴다.
	HashMap<string, std::unique_ptr<BehaviorTree>> _trees;
};
