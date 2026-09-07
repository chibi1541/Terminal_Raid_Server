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
}
