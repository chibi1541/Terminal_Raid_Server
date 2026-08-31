#pragma once
#include "JobQueue.h"

class Room : public JobQueue
{
public:

	Room();
	~Room();

	void Broadcast(SendBufferRef sendBuffer);

	void Tick(float deltaTime);

	void BeginPlay();

private:
	USE_LOCK;
};

extern shared_ptr<Room> GRoom;

