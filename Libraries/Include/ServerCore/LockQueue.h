#pragma once
template<typename T>
class LockQueue
{
public:
	void Push(T item)
	{
		WRITE_LOCK;
		_items.push(item);
	}

	T Pop()
	{
		WRITE_LOCK;
		if(_items.empty())
			return T();

		T ret = _items.front();
		_items.pop();

		return ret;
	}

	// Queue 내부를 Flush하는 쓰레드는 이 함수를 호출해서 Job을 전부 복사하고 Lock을 해제 후 Job을 처리
	void PopAll(OUT Vector<T>& items)
	{
		WRITE_LOCK;
		while(T item = Pop())
		{
			items.push_back(item);
		}
	}

	void Clear()
	{
		WRITE_LOCK;
		_items = Queue<T>();
	}

	/* Monitoring */
	int32 GetSize()
	{
		READ_LOCK;
		return static_cast<int32>(_items.size());
	}


private:
	USE_LOCK;
	Queue<T> _items;

};

