#pragma once

/*---------------
   RefCountable
----------------*/

// 레퍼런스 카운팅 기능을 가진 최상위 클래스
class RefCountable
{
public:
	RefCountable() : _refCount(1) {}
	// 소멸자에 virtual 키워드를 넣어 주지 않으면
	// 상속받은 객체에서 상위 클래스의 소멸자가 불리지 않기 때문에 메모리 누수가 발생
	virtual ~RefCountable() {}

	uint32 GetRefCount() {return _refCount;}
	
	uint32 AddRef() {return ++_refCount;}

	// 해당 객체를 직접 delete하는 것이 아닌 refcount가 0이 되었을 때
	// 스스로 소멸시키는 방식으로 동작
	uint32 ReleaseRef()
	{
		uint32 refCount = --_refCount;
		if(refCount == 0)
		{
			delete this;
		}

		return refCount;
	}

private:
	// 멀티쓰레드에서도 대응하기 위해 atomic 키워드 추가
	atomic<uint32> _refCount;
};

/*---------------
   SharedPtr
----------------*/

template<typename T>
class TSharedPtr
{
public:
	TSharedPtr() {}
	TSharedPtr(T* ptr) { Set(ptr); }
	// 복사 생성자
	TSharedPtr(const TSharedPtr& rhs) { Set(rhs._ptr); }
	// 이동 생성자
	TSharedPtr(TSharedPtr&& rhs) { _ptr = rhs._ptr; rhs._ptr = nullptr; }
	//상속 관계 복사
	template<typename U>
	TSharedPtr(const TSharedPtr<U>& rhs) { Set(static_cast<T*>(rhs._ptr)); }
	
	~TSharedPtr() { Release(); }

public:
	// 복사 대입 연산자
	TSharedPtr& operator=(const TSharedPtr& rhs)
	{
		if(rhs._ptr != _ptr)
		{
			Release();
			Set(rhs._ptr);
		}

		return *this;
	}

	// 이동 대입 연산자
	TSharedPtr& operator=(TSharedPtr&& rhs)
	{
		Release();
		_ptr = rhs._ptr;
		rhs._ptr = nullptr;
		return *this;
	}

	bool operator==(const TSharedPtr& rhs) const {return _ptr == rhs._ptr;}
	bool operator==(T* ptr) const {return _ptr == ptr; }
	bool operator!=(const TSharedPtr& rhs) const {return _ptr != rhs._ptr;}
	bool operator!=(T* ptr) const { return _ptr != ptr; }
	bool operator<(const TSharedPtr& rhs) const { return _ptr < rhs._ptr; }
	T* operator*() {return _ptr;}
	const T* operator*() const { return _ptr; }
	operator T* () const {return _ptr;}
	T* operator->() {return _ptr;}
	const T* operator->() {return _ptr;}

	bool IsNull() {return _ptr == nullptr;}

private:
	// 내부적으로 할당과 해제가 이루지도록 함수를 감쌈
	// 다른 함수에서 빈번하게 호출됨으로 inline 키워드 사용
	inline void Set(T* ptr)
	{
		_ptr = ptr;
		if (ptr)
		{
			ptr->AddRef();
		}
	}

	inline void Release()
	{
		if(_ptr != nullptr)
		{
			_ptr->ReleaseRef();
			_ptr = nullptr;
		}
	}

private:
	T* _ptr;
};


