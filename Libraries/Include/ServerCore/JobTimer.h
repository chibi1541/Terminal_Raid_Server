#pragma once

struct JobData
{
	JobData(weak_ptr<JobQueue> owner, JobRef job) : owner(owner), job(job)
	{
		
	}

	// 혹시나 JobData보다 JobQueue가 먼저 사라지게 되는 경우를 대비해 weak pointer로 참조 
	weak_ptr<JobQueue>	owner;
	JobRef				job;
};

struct TimerItem
{
	inline bool operator<(const TimerItem& otherItem) const
	{
		return executeTick > otherItem.executeTick;
	}

	uint64			executeTick = 0;
	JobData*		jobData = nullptr;
};


/*--------------
	JobTimer
---------------*/

class JobTimer
{
public:
	void		Reserve(uint64 tickAfter, weak_ptr<JobQueue> owner, JobRef job);
	void		Distribute(uint64 now);
	void		Clear();

	/* Monitoring */
	int32		GetReservedCount()
	{
		READ_LOCK;
		return static_cast<int32>(_items.size());
	}


private:
	USE_LOCK;
	PriorityQueue<TimerItem>	_items;
	Atomic<bool>				_distributing = false;
};

