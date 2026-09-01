#pragma once
#include "Game/Bounds.h"

class GameObject;

/*-------------
	QuadTree

	동적 개체 전용 공간 인덱스.
	벽 같은 정적 장애물은 Level / NavGrid 가 셀 격자로 들고 있으므로 여기 넣지 않는다.

	영역 경계에 걸쳐 어느 자식에도 온전히 들어가지 않는 개체는 상위 노드에 남긴다.
	따라서 질의는 노드 자신의 목록을 본 뒤 자식으로도 반드시 재귀해야 한다.

	주의 - 스레드 안전하지 않다. Room 의 JobQueue 안에서만 쓴다.

	주의 - 노드가 GameObject* 를 원시 포인터로 든다.
	매 틱 통째로 다시 만드는 구조라 shared_ptr 로 담으면 개체 수만큼 원자적 refcount
	증감이 매 틱 발생한다. 소유권은 Room::_objects 가 계속 쥐고 있고 트리는 같은 틱
	안에서만 유효하므로 원시 포인터로 충분하다.
	=> 트리에서 꺼낸 포인터를 틱 경계 너머로 들고 가지 말 것.
--------------*/

class QuadTree
{
	enum
	{
		MAX_DEPTH				= 4,	// 120x30 기준 최소 노드 약 7 x 2 셀
		MAX_OBJECTS_PER_NODE	= 4,	// 이 수를 넘으면 분할한다
		DESCRIBE_MAX_NODES		= 24,	// Describe 가 늘어놓을 노드 수 상한
	};

	struct Node
	{
		Bounds				bounds;
		int32				depth = 0;
		Node*				children[4] = {};	// 넷 다 nullptr 이거나 넷 다 유효
		Vector<GameObject*>	objects;			// 이 노드가 온전히 품는 개체들
	};

public:
	~QuadTree();

	// 트리를 비우고 루트를 새 경계로 다시 만든다. 노드는 전부 풀에 반납된다.
	void	Reset(const Bounds& worldBounds);
	void	Insert(GameObject* object);

	// range 와 겹치는 개체를 모은다. out 은 append 가 아니라 덮어쓴다.
	void	QueryRange(const Bounds& range, OUT Vector<GameObject*>& out);
	void	QueryCircle(int32 centerX, int32 centerY, int32 radius, OUT Vector<GameObject*>& out);

	/* Debug */
	int32			GetNodeCount() const		{ return _nodeCount; }
	int32			GetLastVisitedNodes() const	{ return _lastVisited; }
	std::wstring	Describe() const;

private:
	Node*	AllocNode(const Bounds& bounds, int32 depth);
	void	FreeNode(Node* node);	// 자식까지 재귀로 풀에 반납

	void	Insert(Node* node, GameObject* object, const Bounds& objectBounds);
	void	Split(Node* node);
	void	Query(Node* node, const Bounds& range, OUT Vector<GameObject*>& out);

	struct Stats
	{
		int32	nodeCount = 0;
		int32	objectCount = 0;
		int32	straddleCount = 0;			// 자식이 있는 노드에 남은 개체 = 분할선에 걸친 것
		int32	depthNodes[MAX_DEPTH + 1] = {};
		int32	depthNodesWithObjects[MAX_DEPTH + 1] = {};
		int32	emptyNodes = 0;
		int32	listed = 0;
		std::wstring lines;
	};

	void	CollectStats(const Node* node, OUT Stats& stats) const;

private:
	Node*	_root = nullptr;
	int32	_nodeCount = 0;
	int32	_lastVisited = 0;	// 마지막 질의가 방문한 노드 수
};
