#pragma once
#include "Allocator.h"

class MemoryPool;

/*=========================
		MemoryPool
=========================*/
class Memory
{
	enum 
	{
		POOL_COUNT = (1024/32) + (1024/128) + (2048/256),
		MAX_ALLOC_SIZE = 4096
	};

public:
	Memory();
	~Memory();


	void* Allocate(int32 size);
	void Release(void* ptr);

	/* Monitoring : total pool usage (meaningless while _STOMP is defined) */
	void CollectPoolStats(OUT int32& useCount, OUT int32& reserveCount);
	int32 GetPoolCount() const { return static_cast<int32>(_pools.size()); }

private:
	vector<MemoryPool*> _pools;

	MemoryPool* _poolTable[MAX_ALLOC_SIZE + 1];
};


template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	// 여기까지만 하면 메모리 할당만 된 것이지 생성자 및 초기화가 호출되지 않음
	//return memory;

	// replacement new
	// 이미 할당된 메모리 영역으로 생성자를 별로도 호출하는 과정이 필요
	new(memory)Type(std::forward<Args>(args)...);

	// 기존 C++11에서는 모든 매개 변수 타입마다 템플릿을 만들어야 했는데
	// 현재는 위와 같은 가변 인자 처리가 가능함
	// Args&&는 타입 추론이 발생하므로 보편 참조가 됨
	// 보편 참조 : rvlaue -> rvalue, lvalue -> lvalue로 인자가 전달됨
	// 이걸 있는 그대로 받을 수 있는 방법이 std::forward<>
	return memory;
}

template<typename Type>
void xdelete(Type* obj)
{
	// 그냥 이것만 딸랑 호출하면 메모리 해제만 되고 소멸자가 안불리기 때문에
	//BaseAllocator::Release(obj);

	// 명시적으로 호출자를 호출해야 함
	// 평소에는 안부르기 때문에 생소함;;
	obj->~Type();
	PoolAllocator::Release(obj);
}

template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
	return shared_ptr<Type>{ xnew<Type>(forward<Args>(args)...), xdelete<Type> };
}