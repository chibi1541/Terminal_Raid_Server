#pragma once
#include "AI/BtLeaf.h"
#include <functional>

/*------------------
	BtNodeRegistry

	리프 타입 이름 -> 생성 함수.
	캔버스에 적힌 "Wait" 같은 이름을 여기서 찾아 리프 객체를 만든다.

	팩토리가 BehaviorTree 를 받는 이유 :
	파라미터에 적힌 블랙보드 키 이름을 로드 시점에 슬롯 인덱스로 바꿔
	리프 안에 박아두기 위해서다. 그래야 런타임에 이름을 다시 찾지 않는다.

	등록된 이름이 없으면 로더가 그 자리에서 실패시킨다.
	=> 캔버스의 오타를 실행 중이 아니라 로드 시점에 잡는다.
-------------------*/

using BtLeafFactory = std::function<BtLeaf* (const BtParams& params, const BehaviorTree& tree)>;

class BtNodeRegistry
{
public:
	static void			Register(const char* name, BtLeafFactory factory);
	static bool			Contains(const string& name);

	// 실패하면 nullptr. 이름이 없거나 팩토리가 파라미터를 거부한 경우다.
	static BtLeaf*		Create(const string& name, const BtParams& params, const BehaviorTree& tree);

	// 라이브러리의 기본 리프들을 등록한다. main 에서 한 번 부른다.
	static void			RegisterBuiltins();

	/* Debug */
	static std::wstring	DescribeAll();
};
