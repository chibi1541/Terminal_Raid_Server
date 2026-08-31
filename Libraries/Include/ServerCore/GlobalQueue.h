#pragma once
#include "LockQueue.h"

/*----------------
	GlobalQueue
-----------------*/

class GlobalQueue
{
public:
	GlobalQueue();
	~GlobalQueue();

	void Push(JobQueueRef jobQueue);

	JobQueueRef Pop();

	/* Monitoring */
	int32 GetSize() { return _jobQueue.GetSize(); }

private:
	LockQueue<JobQueueRef> _jobQueue;
};

