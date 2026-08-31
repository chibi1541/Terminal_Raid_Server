#pragma once

#include <random>

inline std::mt19937& GetRandomEngine32()
{
	static std::mt19937 engine;
	return engine;
}

inline std::mt19937_64& GetRandomEngine64()
{
	static std::mt19937_64 engine;
	return engine;
}

// 랜덤 시드 설정 함수
inline void SetRandomSeed32()
{
	// 하드웨어 기반 난수 생성기(시드 값 제공
	std::random_device randomDevice;

	// 랜덤 엔진에 종자값 설정
	GetRandomEngine32().seed(randomDevice());
}

inline void SetRandomSeed64()
{
	// 하드웨어 기반 난수 생성기(시드 값 제공
	std::random_device randomDevice;

	// 랜덤 엔진에 종자값 설정
	GetRandomEngine64().seed(randomDevice());
}

// 32비트 정수 난수 함수
inline int32 RandomRange32(int32 min, int32 max)
{
	// min에서 max까지 균등하게 부동소수점 난수를 생성해주는 분포 정의
	std::uniform_int_distribution<int32> distribution(min, max);

	// 난수 반환
	return distribution(GetRandomEngine32());
}

inline int64 RandomRange64(int64 min, int64 max)
{
	std::uniform_int_distribution<int64> distribution(min, max);

	return distribution(GetRandomEngine64());
}

// 부동 소수점 난수 함수
inline float FRandomRange(float min, float max)
{
	// min에서 max까지 균등하게 부동소수점 난수를 생성해주는 분포 정의
	std::uniform_real_distribution<float> distribution(min, max);

	// 난수 반환
	return distribution(GetRandomEngine32());
}


