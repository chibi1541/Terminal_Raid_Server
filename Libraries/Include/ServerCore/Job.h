#pragma once
#include <functional>

/*---------
	Job
----------*/

// 함수인자를 캡쳐로 받을 예정이므로 함수 인자는 필요 없음
using CallbackType = std::function<void()>;

class Job
{
public:
	Job(CallbackType&& callback) : _callback(std::move(callback))
	{
		
	}

	// Args&&... => Variadic Template도 보편 참조가 가능한가 봅니다...
	template<typename T, typename Ret, typename... Args>
	Job(shared_ptr<T> owner, Ret(T::* memFunc)(Args...), Args&&... args)
	{
		_callback = [owner, memFunc, args...]()
		{
			(owner.get()->*memFunc)(args...);
		};
	}

	void Execute()
	{
		_callback();
	}

private:
	CallbackType _callback;
};

