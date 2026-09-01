#include "pch.h"
#include "AI/BtCanvasLoader.h"
#include "AI/BehaviorTree.h"
#include "AI/BtNodeRegistry.h"

// json.hpp 는 1MB 짜리 헤더라 pch 에 넣지 않는다. 이 TU 에서만 쓴다.
#include "json.hpp"

#include <algorithm>
#include <exception>
#include <fstream>

namespace
{
	using json = nlohmann::json;

	const char* BLACKBOARD_NODE_NAME = "Blackboard";

	// [N] 태그가 없는 자식의 순서 키. 태그가 붙은 것보다 항상 뒤로 간다.
	enum { DEFAULT_ORDER = 1000000 };

	void LogFail(const WCHAR* path, const string& message)
	{
		LOG_ERROR(L"[bt] %s : %hs", path, message.c_str());
	}

	string Trim(const string& text)
	{
		size_t begin = 0;
		size_t end = text.size();

		while (begin < end && (text[begin] == ' ' || text[begin] == '\t'
			|| text[begin] == '\r' || text[begin] == '\n'))
		{
			begin++;
		}

		while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t'
			|| text[end - 1] == '\r' || text[end - 1] == '\n'))
		{
			end--;
		}

		return text.substr(begin, end - begin);
	}

	// 줄 앞의 마크다운 장식(# - *)과 공백을 떼어낸다.
	// Obsidian 이 노드 텍스트를 마크다운으로 렌더하기 때문에 붙는 것들이다.
	string StripMarkdown(const string& line)
	{
		size_t begin = 0;

		while (begin < line.size()
			&& (line[begin] == '#' || line[begin] == '-' || line[begin] == '*'
				|| line[begin] == ' ' || line[begin] == '\t'))
		{
			begin++;
		}

		return Trim(line.substr(begin));
	}

	Vector<string> SplitLines(const string& text)
	{
		Vector<string> lines;
		size_t lineStart = 0;

		while (lineStart <= text.size())
		{
			const size_t newlinePos = text.find('\n', lineStart);
			const size_t lineEnd = (newlinePos == string::npos) ? text.size() : newlinePos;

			const string line = StripMarkdown(text.substr(lineStart, lineEnd - lineStart));

			if (line.empty() == false)
				lines.push_back(line);

			if (newlinePos == string::npos)
				break;

			lineStart = newlinePos + 1;
		}

		return lines;
	}

	// "Sequence 추격 [root]" 에서 대괄호 태그를 뽑고 앞쪽 본문을 돌려준다.
	string ExtractTags(const string& text, OUT Vector<string>& outTags)
	{
		string body;
		size_t index = 0;

		while (index < text.size())
		{
			if (text[index] != '[')
			{
				body += text[index];
				index++;
				continue;
			}

			const size_t close = text.find(']', index);

			if (close == string::npos)
			{
				body += text.substr(index);
				break;
			}

			outTags.push_back(Trim(text.substr(index + 1, close - index - 1)));
			index = close + 1;
		}

		return Trim(body);
	}

	bool HasTag(const Vector<string>& tags, const char* name)
	{
		for (const string& tag : tags)
		{
			if (::_stricmp(tag.c_str(), name) == 0)
				return true;
		}

		return false;
	}

	// 첫 토큰만 떼어낸다. 나머지는 사람용 라벨이다.
	string TakeFirstToken(const string& text, OUT string& outRest)
	{
		const size_t space = text.find_first_of(" \t");

		if (space == string::npos)
		{
			outRest.clear();
			return text;
		}

		outRest = Trim(text.substr(space + 1));
		return text.substr(0, space);
	}

	bool ParseNodeType(const string& name, OUT BtNodeType& outType)
	{
		if (name == "Sequence")	{ outType = BtNodeType::Sequence;  return true; }
		if (name == "Selector")	{ outType = BtNodeType::Selector;  return true; }
		if (name == "Inverter")	{ outType = BtNodeType::Inverter;  return true; }
		if (name == "Succeeder"){ outType = BtNodeType::Succeeder; return true; }

		return false;	// 컴포짓이 아니면 리프 후보다
	}

	// "0" / "3.5" / "true" 를 보고 타입을 정한다.
	BbValue ParseBbValue(const string& text)
	{
		if (::_stricmp(text.c_str(), "true") == 0)
			return BbValue::MakeBool(true);

		if (::_stricmp(text.c_str(), "false") == 0)
			return BbValue::MakeBool(false);

		if (text.find('.') != string::npos)
			return BbValue::MakeFloat(static_cast<float>(::atof(text.c_str())));

		return BbValue::MakeInt(::_atoi64(text.c_str()));
	}

	// 캔버스 nodes 배열의 항목 하나.
	struct CanvasNode
	{
		string	id;
		string	text;
		double	x = 0.0;
		double	y = 0.0;
		double	width = 0.0;
		double	height = 0.0;

		double	GetCenterX() const { return x + (width * 0.5); }

		// 파싱 결과
		bool		isBlackboard = false;
		bool		hasRootTag = false;
		BtNodeType	type = BtNodeType::Leaf;
		string		typeName;
		string		label;
		BtParams	params;

		// 트리 구성 중에 채운다
		Vector<int32>	children;		// canvasNodes 인덱스. 정렬 후의 실행 순서
		int32			parentCount = 0;
		int32			flatIndex = -1;	// BehaviorTree::_nodes 에서의 자리
	};

	struct CanvasEdge
	{
		string	fromId;
		string	toId;
		int32	order = DEFAULT_ORDER;
	};
}

/*----------------
	LoadFromFile
-----------------*/

bool BtCanvasLoader::LoadFromFile(const WCHAR* path, const string& name, OUT BehaviorTree& outTree)
{
	/*--- 파일 읽기 ---*/

	// FileUtils 를 쓰지 않는 이유가 두 가지 있다.
	//
	// 1. ServerCore 의 FileUtils.h 는 헤더에 .cpp 내용이 그대로 붙어 있어서,
	//    XmlParser.cpp 말고 다른 TU 에서 include 하면 중복 정의로 링크가 깨진다.
	// 2. FileUtils::ReadFile 은 스트림을 텍스트 모드로 연다. CRLF 가 섞이면
	//    file_size 만큼 읽지 못해 버퍼 뒤에 쓰레기가 남는다. JSON 에는 치명적이다.
	//
	// 그래서 여기서 바이너리로 직접 읽는다. 예외도 여기서 막는다.
	string raw;

	try
	{
		std::ifstream stream(path, std::ios::binary);

		if (stream.is_open() == false)
		{
			LOG_ERROR(L"[bt] cannot open %s", path);
			return false;
		}

		raw.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(L"[bt] cannot read %s : %hs", path, e.what());
		return false;
	}

	if (raw.empty())
	{
		LogFail(path, "file is empty");
		return false;
	}

	const json document = json::parse(raw.begin(), raw.end(), nullptr, false);

	if (document.is_discarded() || document.contains("nodes") == false)
	{
		LogFail(path, "not a valid canvas json");
		return false;
	}

	/*--- 노드 읽기 ---*/

	Vector<CanvasNode> nodes;
	Vector<BbValue> blackboardDefaults;
	Vector<string> blackboardNames;
	HashMap<string, int32> blackboardSlots;

	for (const json& item : document["nodes"])
	{
		// 그룹 노드는 BT 에서 쓰지 않는다. 캔버스를 보기 좋게 묶는 용도로만 두게 한다.
		const string itemType = item.value("type", string("text"));
		if (itemType != "text")
			continue;

		CanvasNode node;
		node.id = item.value("id", string());
		node.text = item.value("text", string());
		node.x = item.value("x", 0.0);
		node.y = item.value("y", 0.0);
		node.width = item.value("width", 0.0);
		node.height = item.value("height", 0.0);

		const Vector<string> lines = SplitLines(node.text);

		if (lines.empty())
			continue;

		Vector<string> tags;
		const string head = ExtractTags(lines[0], OUT tags);

		string rest;
		const string firstToken = TakeFirstToken(head, OUT rest);

		node.typeName = firstToken;
		node.label = rest;
		node.hasRootTag = HasTag(tags, "root");

		// 블랙보드 선언 노드는 트리에 들어가지 않는다.
		if (firstToken == BLACKBOARD_NODE_NAME)
		{
			node.isBlackboard = true;

			for (size_t i = 1; i < lines.size(); i++)
			{
				const size_t colon = lines[i].find(':');

				if (colon == string::npos)
				{
					LogFail(path, "blackboard line needs 'key: value' : " + lines[i]);
					return false;
				}

				const string key = Trim(lines[i].substr(0, colon));
				const string value = Trim(lines[i].substr(colon + 1));

				if (blackboardSlots.find(key) != blackboardSlots.end())
				{
					LogFail(path, "duplicated blackboard key : " + key);
					return false;
				}

				blackboardSlots[key] = static_cast<int32>(blackboardDefaults.size());
				blackboardNames.push_back(key);
				blackboardDefaults.push_back(ParseBbValue(value));
			}

			nodes.push_back(node);
			continue;
		}

		// 컴포짓이 아니면 리프로 본다.
		if (ParseNodeType(firstToken, OUT node.type) == false)
			node.type = BtNodeType::Leaf;

		// 이후 줄은 "키: 값" 파라미터.
		for (size_t i = 1; i < lines.size(); i++)
		{
			const size_t colon = lines[i].find(':');

			if (colon == string::npos)
				continue;

			node.params.Add(Trim(lines[i].substr(0, colon)), Trim(lines[i].substr(colon + 1)));
		}

		nodes.push_back(node);
	}

	if (nodes.empty())
	{
		LogFail(path, "no usable node");
		return false;
	}

	/*--- 엣지 읽기 ---*/

	HashMap<string, int32> idToIndex;

	for (size_t i = 0; i < nodes.size(); i++)
		idToIndex[nodes[i].id] = static_cast<int32>(i);

	Vector<CanvasEdge> edges;

	if (document.contains("edges"))
	{
		for (const json& item : document["edges"])
		{
			CanvasEdge edge;
			edge.fromId = item.value("fromNode", string());
			edge.toId = item.value("toNode", string());

			const string label = item.value("label", string());

			Vector<string> tags;
			ExtractTags(label, OUT tags);

			// 라벨의 [N] 이 자식 순서 오버라이드다.
			for (const string& tag : tags)
			{
				if (tag.empty() == false && ::isdigit(static_cast<unsigned char>(tag[0])))
				{
					edge.order = ::atoi(tag.c_str());
					break;
				}
			}

			edges.push_back(edge);
		}
	}

	/*--- 부모 - 자식 잇기 ---*/

	// 정렬 키를 함께 들고 있다가 나중에 한 번에 정렬한다.
	struct ChildEntry
	{
		int32	nodeIndex = 0;
		int32	order = DEFAULT_ORDER;
		double	centerX = 0.0;
		string	id;
	};

	HashMap<int32, Vector<ChildEntry>> childTable;

	for (const CanvasEdge& edge : edges)
	{
		auto fromIt = idToIndex.find(edge.fromId);
		auto toIt = idToIndex.find(edge.toId);

		// 블랙보드 노드로 들어가거나 나가는 엣지는 무시한다.
		if (fromIt == idToIndex.end() || toIt == idToIndex.end())
			continue;

		if (nodes[fromIt->second].isBlackboard || nodes[toIt->second].isBlackboard)
			continue;

		ChildEntry entry;
		entry.nodeIndex = toIt->second;
		entry.order = edge.order;
		entry.centerX = nodes[toIt->second].GetCenterX();
		entry.id = nodes[toIt->second].id;

		childTable[fromIt->second].push_back(entry);
		nodes[toIt->second].parentCount++;
	}

	// 정렬 키는 (N, 중심 x, 노드 id).
	// 마지막 키가 없으면 Obsidian 이 배열을 뒤섞을 때마다 실행 순서가 바뀐다.
	for (auto& item : childTable)
	{
		Vector<ChildEntry>& list = item.second;

		std::sort(list.begin(), list.end(),
			[](const ChildEntry& a, const ChildEntry& b)
			{
				if (a.order != b.order)
					return a.order < b.order;

				if (a.centerX != b.centerX)
					return a.centerX < b.centerX;

				return a.id < b.id;
			});

		for (const ChildEntry& entry : list)
			nodes[item.first].children.push_back(entry.nodeIndex);
	}

	/*--- 검증 ---*/

	int32 rootIndex = -1;

	for (size_t i = 0; i < nodes.size(); i++)
	{
		const CanvasNode& node = nodes[i];

		if (node.isBlackboard)
			continue;

		if (node.hasRootTag)
		{
			if (rootIndex >= 0)
			{
				LogFail(path, "more than one [root] : " + nodes[rootIndex].typeName + " and " + node.typeName);
				return false;
			}

			rootIndex = static_cast<int32>(i);
		}

		if (node.parentCount > 1)
		{
			LogFail(path, "node has " + std::to_string(node.parentCount) + " parents : " + node.typeName);
			return false;
		}
	}

	if (rootIndex < 0)
	{
		LogFail(path, "no [root] node");
		return false;
	}

	if (nodes[rootIndex].parentCount > 0)
	{
		LogFail(path, "[root] must not have a parent");
		return false;
	}

	// 컴포짓 자식 수
	for (const CanvasNode& node : nodes)
	{
		if (node.isBlackboard)
			continue;

		const int32 childCount = static_cast<int32>(node.children.size());

		switch (node.type)
		{
		case BtNodeType::Sequence:
		case BtNodeType::Selector:
			if (childCount < 1)
			{
				LogFail(path, node.typeName + " needs at least one child");
				return false;
			}
			break;

		case BtNodeType::Inverter:
		case BtNodeType::Succeeder:
			if (childCount != 1)
			{
				LogFail(path, node.typeName + " needs exactly one child, got "
					+ std::to_string(childCount));
				return false;
			}
			break;

		case BtNodeType::Leaf:
			if (childCount != 0)
			{
				LogFail(path, "leaf " + node.typeName + " must not have children");
				return false;
			}

			if (BtNodeRegistry::Contains(node.typeName) == false)
			{
				LogFail(path, "unknown leaf type : " + node.typeName + "   (try 'bt leaves')");
				return false;
			}
			break;
		}
	}

	/*--- 평탄화 : 루트부터 너비 우선. 자식이 연속으로 놓이게 된다 ---*/

	Vector<int32> order;			// flat 순서 -> canvas 인덱스
	order.push_back(rootIndex);

	for (size_t head = 0; head < order.size(); head++)
	{
		const CanvasNode& node = nodes[order[head]];

		for (int32 childIndex : node.children)
		{
			// 사이클이면 이미 자리를 받은 노드가 다시 들어온다.
			if (nodes[childIndex].flatIndex >= 0)
			{
				LogFail(path, "cycle detected at : " + nodes[childIndex].typeName);
				return false;
			}

			nodes[childIndex].flatIndex = 0;	// 자리표. 실제 값은 아래에서 채운다
			order.push_back(childIndex);
		}
	}

	for (size_t i = 0; i < order.size(); i++)
		nodes[order[i]].flatIndex = static_cast<int32>(i);

	// 루트에서 못 닿는 노드가 있으면 캔버스에 엣지를 빠뜨린 것이다.
	for (size_t i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].isBlackboard)
			continue;

		if (nodes[i].flatIndex < 0)
		{
			LogFail(path, "node is not reachable from [root] : " + nodes[i].typeName);
			return false;
		}
	}

	/*--- 트리 채우기 ---*/

	// 여기까지 왔으면 실패하지 않는다. 이제야 기존 트리를 지운다.
	outTree.Clear();
	outTree._name = name;
	outTree._blackboardDefaults = blackboardDefaults;
	outTree._blackboardNames = blackboardNames;
	outTree._blackboardSlots = blackboardSlots;
	outTree._rootIndex = 0;
	outTree._nodes.resize(order.size());

	int32 compositeCount = 0;

	for (size_t i = 0; i < order.size(); i++)
	{
		const CanvasNode& source = nodes[order[i]];
		BehaviorTree::Node& target = outTree._nodes[i];

		target.type = source.type;
		target.label = source.label;
		target.childCount = static_cast<int32>(source.children.size());
		target.firstChild = (target.childCount > 0) ? nodes[source.children[0]].flatIndex : -1;
		target.parent = -1;

		// 재개 지점이 필요한 것은 컴포짓뿐이다. 슬롯도 여기에만 준다.
		if (source.type == BtNodeType::Sequence || source.type == BtNodeType::Selector)
		{
			target.stateSlot = compositeCount;
			compositeCount++;
		}
	}

	for (size_t i = 0; i < order.size(); i++)
	{
		const BehaviorTree::Node& node = outTree._nodes[i];

		for (int32 c = 0; c < node.childCount; c++)
			outTree._nodes[node.firstChild + c].parent = static_cast<int32>(i);
	}

	outTree._compositeCount = compositeCount;

	/*--- 리프 만들기 ---*/

	for (size_t i = 0; i < order.size(); i++)
	{
		const CanvasNode& source = nodes[order[i]];

		if (source.type != BtNodeType::Leaf)
			continue;

		// 팩토리가 트리를 받는 이유 - 파라미터의 블랙보드 키 이름을
		// 여기서 슬롯 인덱스로 바꿔 리프 안에 박아두기 위해서다.
		BtLeaf* leaf = BtNodeRegistry::Create(source.typeName, source.params, outTree);

		if (leaf == nullptr)
		{
			LogFail(path, "leaf factory rejected params : " + source.typeName);
			outTree.Clear();
			return false;
		}

		outTree._nodes[i].leafIndex = static_cast<int32>(outTree._leaves.size());
		outTree._leaves.push_back(leaf);
	}

	LOG_INFO(L"[bt] loaded %s : %d nodes, %d composites, %d blackboard slots",
		path, outTree.GetNodeCount(), outTree.GetCompositeCount(),
		static_cast<int32>(blackboardDefaults.size()));

	return true;
}
