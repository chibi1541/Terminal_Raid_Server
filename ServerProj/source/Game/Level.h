#pragma once
#include "Game/NavGrid.h"

/*----------
	Level

	셀 단위의 진실. XML에서 읽고, 여기서 NavGrid를 굽는다.

	XML 형식 (한 글자가 한 셀, '#'이 장애물)
		<Level levelId="Cemetery" width="120" height="30" tileSize="3">
			<Row>########...</Row>
			...
		</Level>

	levelId : 콘텐츠 식별자. 로드 로그 / 진단용이다. 없으면 빈 문자열.
-----------*/

class Level
{
	enum : WCHAR
	{
		BLOCK_CHAR = L'#',
	};

public:
	bool	LoadFromFile(const WCHAR* path);

	// 로드 실패 시 폴백. 테두리만 벽인 빈 맵을 만든다.
	// 맵 파일 하나 때문에 서버가 안 뜨는 것보다는 낫다.
	void	BuildEmpty(int32 width, int32 height, int32 tileSize);

	const std::wstring&	GetLevelId() const	{ return _levelId; }

	int32	GetWidth() const	{ return _width; }		// 셀 개수
	int32	GetHeight() const	{ return _height; }
	int32	GetTileSize() const	{ return _tileSize; }

	// 범위 밖은 항상 막힌 것으로 취급한다. (레벨의 끝 = 장애물)
	bool	IsCellBlocked(int32 x, int32 y) const;

	// row-major 셀 배열 (0 = 통행 가능, 1 = 장애물). 크기 = width * height.
	// 디버그 오버레이가 서버 충돌 격자를 그대로 클라에 실어 보낼 때 쓴다.
	const Vector<uint8>&	GetCells() const { return _cells; }

	const NavGrid&	GetNavGrid() const { return _navGrid; }

	// cellsWide/High : 이 격자를 쓸 액터의 충돌 박스 한 변(셀). (1,1 = 기본 grid = 원시 셀맵).
	// 셀 (x,y) walkable = 그 셀에 박스를 놓았을 때 벽에 안 걸림 (= MoveMath::BoxBlockedCells).
	// 처음 요청받은 크기만 그때 구워서 캐싱한다. 액터 타입이 몇 종류 안 되므로 상한을 두지 않는다.
	const NavGrid&	GetNavGridForCollisionBox(int32 cellsWide, int32 cellsHigh);

private:
	std::wstring	_levelId;
	int32			_width = 0;
	int32			_height = 0;
	int32			_tileSize = 3;	// S_DEBUG_LEVEL 전송용. 길찾기에선 미사용 (셀 공간 전환).
	Vector<uint8>	_cells;		// 0 = 통행 가능, 1 = 장애물
	NavGrid			_navGrid;

	// 충돌 박스 크기별로 구운 NavGrid 캐시. 키 = (cellsWide << 32) | cellsHigh.
	HashMap<uint64, NavGrid>	_navGridCache;
};
