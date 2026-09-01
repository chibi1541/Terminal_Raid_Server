#pragma once
#include "AI/BtTypes.h"

/*--------------
	Blackboard

	개체 하나가 드는 AI 상태 저장소.

	값은 오직 슬롯 인덱스로만 접근한다.
	슬롯은 트리를 읽을 때 확정되고, 리프는 그 인덱스를 자기 안에 박아둔다.
	=> 런타임에 문자열 해시도, 맵 조회도, 개체별 맵 할당도 없다.
---------------*/

class Blackboard
{
public:
	// 트리가 선언한 기본값을 그대로 복사해 온다. 크기가 여기서 확정된다.
	void	Init(const Vector<BbValue>& defaults);

	int32	GetSlotCount() const { return static_cast<int32>(_values.size()); }
	bool	IsValidSlot(int32 slot) const { return slot >= 0 && slot < GetSlotCount(); }

	int64	GetInt(int32 slot) const;
	float	GetFloat(int32 slot) const;
	bool	GetBool(int32 slot) const;

	void	SetInt(int32 slot, int64 value);
	void	SetFloat(int32 slot, float value);
	void	SetBool(int32 slot, bool value);
	void	Set(int32 slot, const BbValue& value);

	const BbValue& Get(int32 slot) const;

private:
	Vector<BbValue> _values;
};
