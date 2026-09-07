#include "pch.h"
#include "Game/MonsterData.h"
#include "XmlParser.h"

namespace
{
	std::string ToNarrow(const WCHAR* text)
	{
		std::string result;
		if (text != nullptr)
		{
			for (const WCHAR* p = text; *p != 0; ++p)
				result += (*p < 128) ? static_cast<char>(*p) : '?';
		}
		return result;
	}
}

MonsterData& MonsterData::Get()
{
	static MonsterData instance;
	return instance;
}

bool MonsterData::LoadFromFile(const WCHAR* path)
{
	XmlParser parser;
	XmlNode root;

	if (parser.ParseFromFile(path, OUT root) == false || root.IsValid() == false)
	{
		LOG_ERROR(L"[monster] failed to parse : %s (컴파일 기본값으로 진행)", path);
		return false;
	}

	_defs.clear();

	for (XmlNode& node : root.FindChildren(L"Monster"))
	{
		MonsterDef def;
		def.id = ToNarrow(node.GetStringAttr(L"id"));
		if (def.id.empty())
			continue;

		def.footprintTiles = node.GetInt32Attr(L"footprintTiles", 2);
		def.collisionCells = node.GetInt32Attr(L"collisionCells", 8);
		def.radius = node.GetInt32Attr(L"radius", 4);
		def.maxHp = node.GetInt32Attr(L"maxHp", 50);

		_defs[def.id] = def;
	}

	LOG_INFO(L"[monster] loaded %s : %d defs", path, static_cast<int32>(_defs.size()));

	return true;
}

const MonsterDef& MonsterData::Find(const std::string& id) const
{
	const auto it = _defs.find(id);
	return (it != _defs.end()) ? it->second : _fallback;
}
