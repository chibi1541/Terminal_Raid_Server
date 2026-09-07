#include "pch.h"
#include "Game/ProjectileData.h"
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

	Protocol::ProjectileType ParseType(const WCHAR* text)
	{
		Protocol::ProjectileType value = Protocol::Projectile_None;
		Protocol::ProjectileType_Parse(ToNarrow(text), &value);
		return value;
	}
}

ProjectileData& ProjectileData::Get()
{
	static ProjectileData instance;
	return instance;
}

bool ProjectileData::LoadFromFile(const WCHAR* path)
{
	XmlParser parser;
	XmlNode root;

	if (parser.ParseFromFile(path, OUT root) == false || root.IsValid() == false)
	{
		LOG_ERROR(L"[projectile] failed to parse : %s (컴파일 기본값으로 진행)", path);
		return false;
	}

	const Protocol::ProjectileType parsedDefault = ParseType(root.GetStringAttr(L"defaultProjectile"));
	if (parsedDefault != Protocol::Projectile_None)
		_defaultType = parsedDefault;

	_defs.clear();

	for (XmlNode& node : root.FindChildren(L"Projectile"))
	{
		ProjectileDef def;
		def.type = ParseType(node.GetStringAttr(L"id"));
		if (def.type == Protocol::Projectile_None)
			continue;

		def.speedCellsPerSec = node.GetInt32Attr(L"speedCellsPerSec", 60);
		def.rangeCells = node.GetInt32Attr(L"rangeCells", 40);
		def.radius = node.GetInt32Attr(L"radius", 1);
		def.fireIntervalMs = node.GetInt32Attr(L"fireIntervalMs", 250);
		def.spawnForwardCells = node.GetInt32Attr(L"spawnForwardCells", 3);
		def.spawnUpCells = node.GetInt32Attr(L"spawnUpCells", 6);

		_defs[static_cast<int>(def.type)] = def;
	}

	LOG_INFO(L"[projectile] loaded %s : %d defs, default=%d",
		path, static_cast<int32>(_defs.size()), static_cast<int32>(_defaultType));

	return true;
}

const ProjectileDef& ProjectileData::Find(Protocol::ProjectileType type) const
{
	const auto it = _defs.find(static_cast<int>(type));
	return (it != _defs.end()) ? it->second : _fallback;
}
