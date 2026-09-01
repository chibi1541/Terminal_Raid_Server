#pragma once

/*-----------
	Bounds

	셀 단위 정수 경계. [min, max] 폐구간이다.
	셀 인덱스가 정수라서 반개구간보다 폐구간이 헷갈림이 적다.
	즉 minX == maxX 이면 폭이 1셀이라는 뜻이다.
------------*/

struct Bounds
{
	int32 minX = 0;
	int32 minY = 0;
	int32 maxX = 0;
	int32 maxY = 0;

	static Bounds Make(int32 minX, int32 minY, int32 maxX, int32 maxY)
	{
		Bounds bounds;
		bounds.minX = minX;
		bounds.minY = minY;
		bounds.maxX = maxX;
		bounds.maxY = maxY;

		return bounds;
	}

	static Bounds FromCircle(int32 centerX, int32 centerY, int32 radius)
	{
		return Make(centerX - radius, centerY - radius, centerX + radius, centerY + radius);
	}

	int32 GetWidth() const	{ return maxX - minX + 1; }
	int32 GetHeight() const	{ return maxY - minY + 1; }

	// other 까지 품도록 자신을 넓힌다.
	void Encapsulate(const Bounds& other)
	{
		if (other.minX < minX) minX = other.minX;
		if (other.minY < minY) minY = other.minY;
		if (other.maxX > maxX) maxX = other.maxX;
		if (other.maxY > maxY) maxY = other.maxY;
	}

	// other를 남김없이 품는가
	bool Contains(const Bounds& other) const
	{
		return minX <= other.minX && maxX >= other.maxX
			&& minY <= other.minY && maxY >= other.maxY;
	}

	bool Contains(int32 x, int32 y) const
	{
		return x >= minX && x <= maxX && y >= minY && y <= maxY;
	}

	// 한 칸이라도 겹치는가
	bool Intersects(const Bounds& other) const
	{
		if (other.minX > maxX || other.maxX < minX)
			return false;

		if (other.minY > maxY || other.maxY < minY)
			return false;

		return true;
	}
};
