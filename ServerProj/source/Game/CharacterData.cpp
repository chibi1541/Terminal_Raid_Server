#include "pch.h"
#include "Game/CharacterData.h"
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

	Protocol::CharacterType ParseType(const WCHAR* text)
	{
		Protocol::CharacterType value = Protocol::CHARACTER_NONE;
		Protocol::CharacterType_Parse(ToNarrow(text), &value);
		return value;
	}
}

CharacterData& CharacterData::Get()
{
	static CharacterData instance;
	return instance;
}

bool CharacterData::LoadFromFile(const WCHAR* path)
{
	XmlParser parser;
	XmlNode root;

	if (parser.ParseFromFile(path, OUT root) == false || root.IsValid() == false)
	{
		LOG_ERROR(L"[character] failed to parse : %s (컴파일 기본값으로 진행)", path);
		return false;
	}

	const Protocol::CharacterType parsedDefault = ParseType(root.GetStringAttr(L"defaultCharacter"));
	if (parsedDefault != Protocol::CHARACTER_NONE)
		_defaultType = parsedDefault;

	_defs.clear();

	for (XmlNode& node : root.FindChildren(L"Character"))
	{
		CharacterDef def;
		def.type = ParseType(node.GetStringAttr(L"id"));
		if (def.type == Protocol::CHARACTER_NONE)
			continue;

		def.animClip = ToNarrow(node.GetStringAttr(L"animClip", L"Knight"));
		def.collisionCells = node.GetInt32Attr(L"collisionCells", 8);
		def.radius = node.GetInt32Attr(L"radius", 2);
		def.maxHp = node.GetInt32Attr(L"maxHp", 100);
		def.attackPower = node.GetInt32Attr(L"attackPower", 10);
		def.moveSpeedCells = node.GetInt32Attr(L"moveSpeedCells", 20);
		def.hitStunMs = node.GetInt32Attr(L"hitStunMs", 300);

		_defs[static_cast<int>(def.type)] = def;
	}

	LOG_INFO(L"[character] loaded %s : %d defs, default=%d",
		path, static_cast<int32>(_defs.size()), static_cast<int32>(_defaultType));

	return true;
}

const CharacterDef& CharacterData::Find(Protocol::CharacterType type) const
{
	const auto it = _defs.find(static_cast<int>(type));
	return (it != _defs.end()) ? it->second : _fallback;
}
