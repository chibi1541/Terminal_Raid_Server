#include "pch.h"
#include "Game/Level.h"
#include "XmlParser.h"

bool Level::LoadFromFile(const WCHAR* path)
{
	XmlParser parser;
	XmlNode root;

	if (parser.ParseFromFile(path, OUT root) == false || root.IsValid() == false)
	{
		LOG_ERROR(L"[level] failed to parse : %s", path);
		return false;
	}

	const WCHAR* levelId = root.GetStringAttr(L"levelId");
	const int32 width = root.GetInt32Attr(L"width");
	const int32 height = root.GetInt32Attr(L"height");
	const int32 tileSize = root.GetInt32Attr(L"tileSize", 3);

	if (width <= 0 || height <= 0 || tileSize <= 0)
	{
		LOG_ERROR(L"[level] invalid header : width=%d height=%d tileSize=%d", width, height, tileSize);
		return false;
	}

	Vector<XmlNode> rows = root.FindChildren(L"Row");

	if (static_cast<int32>(rows.size()) != height)
	{
		LOG_ERROR(L"[level] row count mismatch : got %d, expected %d",
			static_cast<int32>(rows.size()), height);
		return false;
	}

	Vector<uint8> cells;
	cells.resize(static_cast<size_t>(width) * height, 0);

	for (int32 y = 0; y < height; y++)
	{
		const WCHAR* line = rows[y].GetStringValue();
		const int32 length = static_cast<int32>(::wcslen(line));

		// 길이가 어긋나면 어느 행이 몇 글자인지 정확히 알려준다.
		// 이걸 안 찍어두면 맵이 조용히 밀린 채로 로드되어 길찾기 버그로 오해하게 된다.
		if (length != width)
		{
			LOG_ERROR(L"[level] row %d length mismatch : got %d, expected %d", y, length, width);
			return false;
		}

		for (int32 x = 0; x < width; x++)
			cells[static_cast<size_t>(y) * width + x] = (line[x] == BLOCK_CHAR) ? 1 : 0;
	}

	_levelId = levelId;
	_width = width;
	_height = height;
	_tileSize = tileSize;
	_cells = std::move(cells);
	_navGrid.Build(_cells, _width, _height, _tileSize, _tileSize);

	LOG_INFO(L"[level] loaded %s (id=%s) : %d x %d cells, tileSize %d -> %d x %d tiles",
		path, _levelId.empty() ? L"(none)" : _levelId.c_str(),
		_width, _height, _tileSize, _navGrid.GetWidth(), _navGrid.GetHeight());

	return true;
}

void Level::BuildEmpty(int32 width, int32 height, int32 tileSize)
{
	_levelId = L"(empty)";
	_width = (width > 0) ? width : 1;
	_height = (height > 0) ? height : 1;
	_tileSize = (tileSize > 0) ? tileSize : 1;

	_cells.clear();
	_cells.resize(static_cast<size_t>(_width) * _height, 0);

	// 테두리만 벽
	for (int32 x = 0; x < _width; x++)
	{
		_cells[x] = 1;
		_cells[static_cast<size_t>(_height - 1) * _width + x] = 1;
	}

	for (int32 y = 0; y < _height; y++)
	{
		_cells[static_cast<size_t>(y) * _width] = 1;
		_cells[static_cast<size_t>(y) * _width + (_width - 1)] = 1;
	}

	_navGrid.Build(_cells, _width, _height, _tileSize, _tileSize);

	LOG_WARN(L"[level] built empty fallback : %d x %d cells -> %d x %d tiles",
		_width, _height, _navGrid.GetWidth(), _navGrid.GetHeight());
}

bool Level::IsCellBlocked(int32 x, int32 y) const
{
	if (x < 0 || y < 0 || x >= _width || y >= _height)
		return true;

	return _cells[static_cast<size_t>(y) * _width + x] != 0;
}

const NavGrid& Level::GetNavGridForFootprint(int32 footprintTilesWide, int32 footprintTilesHigh)
{
	footprintTilesWide = (footprintTilesWide > 0) ? footprintTilesWide : 1;
	footprintTilesHigh = (footprintTilesHigh > 0) ? footprintTilesHigh : 1;

	if (footprintTilesWide == 1 && footprintTilesHigh == 1)
		return _navGrid;

	const uint64 key = (static_cast<uint64>(footprintTilesWide) << 32)
		| static_cast<uint32>(footprintTilesHigh);

	auto it = _footprintNavGrids.find(key);
	if (it != _footprintNavGrids.end())
		return it->second;

	NavGrid grid;
	grid.Build(_cells, _width, _height,
		_tileSize * footprintTilesWide, _tileSize * footprintTilesHigh);

	return _footprintNavGrids.emplace(key, std::move(grid)).first->second;
}
