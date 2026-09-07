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

	Protocol::MonsterType ParseType(const WCHAR* text)
	{
		Protocol::MonsterType value = Protocol::Monster_None;
		Protocol::MonsterType_Parse(ToNarrow(text), &value);
		return value;
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

	const Protocol::MonsterType parsedDefault = ParseType(root.GetStringAttr(L"defaultMonster"));
	if (parsedDefault != Protocol::Monster_None)
		_defaultType = parsedDefault;

	_defs.clear();

	for (XmlNode& node : root.FindChildren(L"Monster"))
	{
		MonsterDef def;
		def.type = ParseType(node.GetStringAttr(L"id"));
		if (def.type == Protocol::Monster_None)
			continue;

		def.animClip = ToNarrow(node.GetStringAttr(L"animClip", L"Zombie"));
		def.aiTree = ToNarrow(node.GetStringAttr(L"aiTree", L"monster_basic"));
		def.footprintTiles = node.GetInt32Attr(L"footprintTiles", 2);
		def.collisionCells = node.GetInt32Attr(L"collisionCells", 8);
		def.radius = node.GetInt32Attr(L"radius", 4);
		def.maxHp = node.GetInt32Attr(L"maxHp", 60);
		def.attackPower = node.GetInt32Attr(L"attackPower", 8);
		def.moveSpeedCells = node.GetInt32Attr(L"moveSpeedCells", 8);
		def.hitStunMs = node.GetInt32Attr(L"hitStunMs", 0);
		def.deathFadeMs = node.GetInt32Attr(L"deathFadeMs", 0);

		_defs[static_cast<int>(def.type)] = def;
	}

	LOG_INFO(L"[monster] loaded %s : %d defs, default=%d",
		path, static_cast<int32>(_defs.size()), static_cast<int32>(_defaultType));

	return true;
}

const MonsterDef& MonsterData::Find(Protocol::MonsterType type) const
{
	const auto it = _defs.find(static_cast<int>(type));
	return (it != _defs.end()) ? it->second : _fallback;
}
