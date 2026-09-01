#include "pch.h"
#include "Game/QuadTree.h"
#include "Game/GameObject.h"

QuadTree::~QuadTree()
{
	if (_root != nullptr)
	{
		FreeNode(_root);
		_root = nullptr;
	}
}

/*-------------
	노드 풀링
--------------*/

QuadTree::Node* QuadTree::AllocNode(const Bounds& bounds, int32 depth)
{
	// Debug 빌드는 _STOMP 가 켜져 있어 ObjectPool 이 풀을 우회하고 StompAllocator 로 간다.
	// 풀링 효과를 보려면 Release 로 확인할 것.
	Node* node = ObjectPool<Node>::Pop();

	node->bounds = bounds;
	node->depth = depth;

	_nodeCount++;

	return node;
}

void QuadTree::FreeNode(Node* node)
{
	if (node == nullptr)
		return;

	for (int32 i = 0; i < 4; i++)
	{
		FreeNode(node->children[i]);
		node->children[i] = nullptr;
	}

	_nodeCount--;

	ObjectPool<Node>::Push(node);
}

/*----------
	재구축
-----------*/

void QuadTree::Reset(const Bounds& worldBounds)
{
	if (_root != nullptr)
		FreeNode(_root);

	_nodeCount = 0;
	_lastVisited = 0;
	_root = AllocNode(worldBounds, 0);
}

void QuadTree::Insert(GameObject* object)
{
	if (_root == nullptr || object == nullptr)
		return;

	// 월드 밖으로 삐져나온 개체는 어느 자식에도 온전히 안 들어가므로 루트에 남는다.
	// 질의에서 누락되지는 않으니 그대로 둔다.
	Insert(_root, object, object->GetBounds());
}

void QuadTree::Insert(Node* node, GameObject* object, const Bounds& objectBounds)
{
	if (node->children[0] != nullptr)
	{
		for (int32 i = 0; i < 4; i++)
		{
			if (node->children[i]->bounds.Contains(objectBounds))
			{
				Insert(node->children[i], object, objectBounds);
				return;
			}
		}

		// 어느 자식에도 온전히 안 들어간다 = 분할선에 걸쳤다. 상위 노드인 여기에 남긴다.
		node->objects.push_back(object);
		return;
	}

	node->objects.push_back(object);

	if (static_cast<int32>(node->objects.size()) > MAX_OBJECTS_PER_NODE && node->depth < MAX_DEPTH)
		Split(node);
}

void QuadTree::Split(Node* node)
{
	// 폭이나 높이가 1셀이면 더 쪼갤 수 없다.
	// 이 검사가 없으면 min > max 인 뒤집힌 자식이 만들어진다.
	if (node->bounds.GetWidth() < 2 || node->bounds.GetHeight() < 2)
		return;

	const Bounds& b = node->bounds;
	const int32 midX = b.minX + (b.maxX - b.minX) / 2;
	const int32 midY = b.minY + (b.maxY - b.minY) / 2;
	const int32 childDepth = node->depth + 1;

	node->children[0] = AllocNode(Bounds::Make(b.minX,    b.minY,    midX,   midY),   childDepth);
	node->children[1] = AllocNode(Bounds::Make(midX + 1,  b.minY,    b.maxX, midY),   childDepth);
	node->children[2] = AllocNode(Bounds::Make(b.minX,    midY + 1,  midX,   b.maxY), childDepth);
	node->children[3] = AllocNode(Bounds::Make(midX + 1,  midY + 1,  b.maxX, b.maxY), childDepth);

	// 기존 개체 중 자식에 온전히 들어가는 것만 내려보내고 나머지는 여기 남긴다.
	// 임시 벡터를 쓰지 않고 제자리에서 앞으로 당긴다.
	// (StlAllocator 에 operator== 가 없어서 vector::swap 이 컴파일되지 않는다)
	size_t write = 0;

	for (size_t read = 0; read < node->objects.size(); read++)
	{
		GameObject* object = node->objects[read];
		const Bounds objectBounds = object->GetBounds();

		int32 childIndex = -1;

		for (int32 i = 0; i < 4; i++)
		{
			if (node->children[i]->bounds.Contains(objectBounds))
			{
				childIndex = i;
				break;
			}
		}

		if (childIndex >= 0)
		{
			Insert(node->children[childIndex], object, objectBounds);
			continue;
		}

		node->objects[write] = object;
		write++;
	}

	node->objects.resize(write);
}

/*--------
	질의
---------*/

void QuadTree::QueryRange(const Bounds& range, OUT Vector<GameObject*>& out)
{
	out.clear();
	_lastVisited = 0;

	Query(_root, range, OUT out);
}

void QuadTree::Query(Node* node, const Bounds& range, OUT Vector<GameObject*>& out)
{
	if (node == nullptr || node->bounds.Intersects(range) == false)
		return;

	_lastVisited++;

	for (GameObject* object : node->objects)
	{
		if (object->GetBounds().Intersects(range))
			out.push_back(object);
	}

	// 걸친 개체가 상위 노드에 있으므로, 자기 목록을 본 뒤에도 자식으로 반드시 내려가야 한다.
	for (int32 i = 0; i < 4; i++)
		Query(node->children[i], range, OUT out);
}

void QuadTree::QueryCircle(int32 centerX, int32 centerY, int32 radius, OUT Vector<GameObject*>& out)
{
	QueryRange(Bounds::FromCircle(centerX, centerY, radius), OUT out);

	// AABB 로 추린 결과를 원 판정으로 한 번 더 거른다. 제자리 압축이라 별도 버퍼가 필요 없다.
	size_t write = 0;

	for (size_t read = 0; read < out.size(); read++)
	{
		if (out[read]->OverlapsCircle(centerX, centerY, radius))
		{
			out[write] = out[read];
			write++;
		}
	}

	out.resize(write);
}

/*-------------
	Debug 출력
--------------*/

void QuadTree::CollectStats(const Node* node, OUT Stats& stats) const
{
	if (node == nullptr)
		return;

	const bool internal = (node->children[0] != nullptr);
	const int32 count = static_cast<int32>(node->objects.size());

	stats.nodeCount++;
	stats.objectCount += count;

	if (node->depth >= 0 && node->depth <= MAX_DEPTH)
	{
		stats.depthNodes[node->depth]++;

		if (count > 0)
			stats.depthNodesWithObjects[node->depth]++;
	}

	if (count == 0)
		stats.emptyNodes++;

	if (internal)
		stats.straddleCount += count;

	if (count > 0 && stats.listed < DESCRIBE_MAX_NODES)
	{
		WCHAR buffer[256];
		::swprintf_s(buffer, L"\n  d%d [%d,%d]-[%d,%d] : %d objects%s",
			node->depth, node->bounds.minX, node->bounds.minY,
			node->bounds.maxX, node->bounds.maxY, count,
			internal ? L"  <- straddling" : L"");

		stats.lines += buffer;
		stats.listed++;
	}

	for (int32 i = 0; i < 4; i++)
		CollectStats(node->children[i], OUT stats);
}

std::wstring QuadTree::Describe() const
{
	Stats stats;
	CollectStats(_root, OUT stats);

	WCHAR buffer[256];
	::swprintf_s(buffer,
		L"quadtree : %d nodes (%d empty), %d objects, %d straddling (stuck in internal nodes)",
		stats.nodeCount, stats.emptyNodes, stats.objectCount, stats.straddleCount);

	std::wstring result = buffer;

	for (int32 depth = 0; depth <= MAX_DEPTH; depth++)
	{
		if (stats.depthNodes[depth] == 0)
			continue;

		::swprintf_s(buffer, L"\n  depth %d : %d nodes, %d holding objects",
			depth, stats.depthNodes[depth], stats.depthNodesWithObjects[depth]);
		result += buffer;
	}

	if (stats.lines.empty() == false)
		result += L"\n  nodes holding objects (empty nodes are not listed) :";

	result += stats.lines;

	if (stats.listed >= DESCRIBE_MAX_NODES)
		result += L"\n  ... (more nodes omitted)";

	return result;
}
