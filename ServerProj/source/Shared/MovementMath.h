#pragma once
#include <cstdint>

// ============================================================================
//  MovementMath - 클라이언트 / 서버 공유 이동 적분식
//
//  ★ 이 파일은 Client\GameProj\source\Shared\MovementMath.h 와 바이트 단위로 동일해야 한다. ★
//  한쪽만 고치면 예측(클라)과 권위 시뮬(서버)의 좌표가 어긋나 재조정이 깨진다.
//
//  좌표는 1/256 셀 고정소수점(서브유닛)으로 굴린다. 정수 나눗셈의 순서/자릿수까지
//  양쪽이 똑같이 밟도록 여기 한 곳에 모아 둔다.
// ============================================================================

namespace MoveMath
{
	constexpr int32_t POS_SHIFT = 8;
	constexpr int32_t POS_SCALE = 1 << POS_SHIFT;					// 256 서브유닛 = 1 셀
	constexpr int32_t DEFAULT_MOVE_SPEED_CELLS = 20;					// 기본 이동 속도 (셀/초)
	constexpr int32_t DEFAULT_MOVE_SPEED_SUBUNITS =
		DEFAULT_MOVE_SPEED_CELLS * POS_SCALE;						// 1536 서브유닛/초

	// 대각선 보정 계수 ≈ 1/√2 를 정수로. (181 / 256 = 0.70703125)
	constexpr int32_t DIAG_NUM = 181;
	constexpr int32_t DIAG_DEN = 256;

	// 플레이어가 차지하는 타일 수 (가로 x 세로). 서버 Player::Player 와 클라 예측이 공유한다.
	// 셀 박스 = (WIDE*tileSize) x (HIGH*tileSize), 중심은 캐릭터 위치.
	constexpr int32_t PLAYER_FOOTPRINT_TILES_WIDE = 2;
	constexpr int32_t PLAYER_FOOTPRINT_TILES_HIGH = 1;

	// 단위 방향벡터 (ux, uy ∈ {-1, 0, 1}) 로 speedSubunitsPerSec 속도로 elapsedMs 동안
	// 이동한 고정소수점 변위를 (outDx, outDy) 에 채운다.
	//
	// 중간 연산은 int64 로 한다 - heldMs 가 클 수 있고(정확 catch-up),
	// mag * DIAG_NUM 이 int32 를 넘길 수 있다.
	inline void StepFixed(int32_t ux, int32_t uy,
		int32_t speedSubunitsPerSec, int32_t elapsedMs,
		int32_t& outDx, int32_t& outDy)
	{
		if (elapsedMs <= 0 || (ux == 0 && uy == 0))
		{
			outDx = 0;
			outDy = 0;
			return;
		}

		int64_t mag = static_cast<int64_t>(speedSubunitsPerSec) * elapsedMs / 1000;

		// 대각은 두 축 모두 움직이므로 총 속력이 √2 배가 되지 않도록 스칼라를 줄인다.
		if (ux != 0 && uy != 0)
			mag = mag * DIAG_NUM / DIAG_DEN;

		outDx = static_cast<int32_t>(ux * mag);
		outDy = static_cast<int32_t>(uy * mag);
	}

	// 충돌 슬라이드를 나눌 조각 크기(서브유닛). 0.5셀. 한 조각이 이 이상 움직이지 않으면
	// 목적 셀만 검사해도 셀을 건너뛰지 않는다.
	constexpr int32_t SUBSTEP_SUBUNITS = 128;

	// 한 스텝(stepX, stepY 서브유닛)을 fp 에 더하되 막힌 셀 중심에는 못 들어간다.
	// 축을 분리해 슬라이드하고 코너컷(대각으로 두 벽 사이를 파고드는 것)을 막는다.
	// isBlocked(cellX, cellY) -> true 면 그 셀은 통행 불가.
	//
	// 서버 Room::IntegrateActor 와 클라 LocalPlayer 예측이 이 한 구현을 공유한다.
	// (위치는 항상 >= 0 이라 fp >> POS_SHIFT 의 산술 시프트 부호 문제 없음)
	template <typename BlockedFn>
	inline void SlideStep(int32_t& fpX, int32_t& fpY,
		int32_t stepX, int32_t stepY, BlockedFn&& isBlocked)
	{
		const int32_t cx = fpX >> POS_SHIFT;
		const int32_t cy = fpY >> POS_SHIFT;

		int32_t nx = fpX + stepX;
		int32_t ny = fpY + stepY;
		int32_t ncx = nx >> POS_SHIFT;
		int32_t ncy = ny >> POS_SHIFT;

		// X축이 벽에 막히면 X만 되돌리고 Y로 슬라이드.
		if (ncx != cx && isBlocked(ncx, cy))
		{
			nx = fpX;
			ncx = cx;
		}

		// Y축도 동일.
		if (ncy != cy && isBlocked(cx, ncy))
		{
			ny = fpY;
			ncy = cy;
		}

		// 두 축이 다 살아 대각으로 들어가는데 목적 칸이 막혔으면 코너컷이다. Y를 죽인다.
		if (ncx != cx && ncy != cy && isBlocked(ncx, ncy))
		{
			ny = fpY;
			ncy = cy;
		}

		fpX = nx;
		fpY = ny;
	}

	// centerCell 을 중심으로 (tilesWide x tilesHigh) 타일 박스 안에 막힌 셀이 하나라도 있으면 true.
	// = 서버 Room::IsFootprintBlocked. tilesWide/High <= 1 이면 단일 셀만 본다.
	// isCellBlocked(cellX, cellY) -> true 면 그 셀 통행 불가.
	template <typename CellBlockedFn>
	inline bool FootprintBlocked(int32_t centerX, int32_t centerY,
		int32_t tilesWide, int32_t tilesHigh, int32_t tileSize, CellBlockedFn&& isCellBlocked)
	{
		if (tilesWide <= 1 && tilesHigh <= 1)
			return isCellBlocked(centerX, centerY);

		const int32_t cellsWide = tilesWide * tileSize;
		const int32_t cellsHigh = tilesHigh * tileSize;

		// 캐릭터 위치를 중심으로 대칭. 짝수라 안 나뉘면 오른쪽/아래쪽에 한 칸 더(서버와 동일).
		const int32_t minX = centerX - cellsWide / 2;
		const int32_t maxX = minX + cellsWide - 1;
		const int32_t minY = centerY - cellsHigh / 2;
		const int32_t maxY = minY + cellsHigh - 1;

		for (int32_t y = minY; y <= maxY; ++y)
		{
			for (int32_t x = minX; x <= maxX; ++x)
			{
				if (isCellBlocked(x, y))
					return true;
			}
		}

		return false;
	}

	// dir(ux,uy) 로 elapsedMs 동안 이동한 StepFixed 변위를 <=0.5셀 조각으로 나눠 SlideStep.
	// 조각 합은 반올림 오차 없이 정확히 총변위라 벽이 없으면 StepFixed 단일 적용과 같다.
	// 서버 Room::IntegrateHeld / Room::UpdateMovement 와 클라 replay 가 공유한다.
	template <typename BlockedFn>
	inline void IntegrateSlide(int32_t& fpX, int32_t& fpY,
		int32_t ux, int32_t uy,
		int32_t speedSubunitsPerSec, int32_t elapsedMs, BlockedFn&& isBlocked)
	{
		int32_t totalX = 0;
		int32_t totalY = 0;
		StepFixed(ux, uy, speedSubunitsPerSec, elapsedMs, totalX, totalY);

		if (totalX == 0 && totalY == 0)
			return;

		const int32_t absX = (totalX < 0) ? -totalX : totalX;
		const int32_t absY = (totalY < 0) ? -totalY : totalY;
		const int32_t span = (absX > absY) ? absX : absY;

		const int32_t steps = (span <= SUBSTEP_SUBUNITS)
			? 1
			: (span + SUBSTEP_SUBUNITS - 1) / SUBSTEP_SUBUNITS;

		int32_t doneX = 0;
		int32_t doneY = 0;
		for (int32_t i = 1; i <= steps; ++i)
		{
			const int32_t wantX = static_cast<int32_t>(static_cast<int64_t>(totalX) * i / steps);
			const int32_t wantY = static_cast<int32_t>(static_cast<int64_t>(totalY) * i / steps);
			SlideStep(fpX, fpY, wantX - doneX, wantY - doneY, isBlocked);
			doneX = wantX;
			doneY = wantY;
		}
	}

	// 임의 각도 속도 벡터(velSubX/Y, 서브유닛/초)로 elapsedMs 동안 이동.
	// <=0.5셀 조각으로 나눠 각 조각의 목적 셀이 막혔으면 거기서 멈추고 true(벽 히트) 리턴.
	// 슬라이드하지 않는다 - 투사체용. 서버 Room::UpdateMovement 가 쓴다.
	template <typename BlockedFn>
	inline bool IntegrateVec(int32_t& fpX, int32_t& fpY,
		int32_t velSubX, int32_t velSubY, int32_t elapsedMs, BlockedFn&& isBlocked)
	{
		if (elapsedMs <= 0 || (velSubX == 0 && velSubY == 0))
			return false;

		const int64_t totalX = static_cast<int64_t>(velSubX) * elapsedMs / 1000;
		const int64_t totalY = static_cast<int64_t>(velSubY) * elapsedMs / 1000;

		const int64_t absX = (totalX < 0) ? -totalX : totalX;
		const int64_t absY = (totalY < 0) ? -totalY : totalY;
		const int64_t span = (absX > absY) ? absX : absY;

		const int32_t steps = (span <= SUBSTEP_SUBUNITS)
			? 1
			: static_cast<int32_t>((span + SUBSTEP_SUBUNITS - 1) / SUBSTEP_SUBUNITS);

		int64_t doneX = 0;
		int64_t doneY = 0;
		for (int32_t i = 1; i <= steps; ++i)
		{
			const int64_t wantX = totalX * i / steps;
			const int64_t wantY = totalY * i / steps;
			const int32_t nx = fpX + static_cast<int32_t>(wantX - doneX);
			const int32_t ny = fpY + static_cast<int32_t>(wantY - doneY);

			if (isBlocked(nx >> POS_SHIFT, ny >> POS_SHIFT))
				return true;	// 이 조각의 목적 셀이 벽. fp 는 직전 값 그대로 두고 멈춘다.

			fpX = nx;
			fpY = ny;
			doneX = wantX;
			doneY = wantY;
		}

		return false;
	}
}
