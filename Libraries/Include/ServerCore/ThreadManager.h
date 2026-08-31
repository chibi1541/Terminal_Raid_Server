#pragma once

#include <thread>
#include <functional>

/*=====================
	ThreadManager
=======================*/

class ThreadManager
{
public:
	ThreadManager();
	~ThreadManager();

	void Launch(function<void(void)> callback);
	void Join();

	static void InitTLS();
	static void DestoryTLS();

	static void DoGlobalQueueWork();
	static void DistributeReservedJobs();

private:
	vector<thread> _threads;
	Mutex _mutex;

};

