#include "pch.h"
#include "Room.h"
#include "Protocol/ClientPacketHandler.h"
#include "GameSession.h"


shared_ptr<Room> GRoom = make_shared<Room>();

Room::Room()
{
}

Room::~Room()
{
}


void Room::Broadcast(SendBufferRef sendBuffer)
{

}


void Room::Tick(float deltaTime)
{
	//uint64 elapsedTime = (_prevElapsedTime != 0) ? (GetTickCount64() - _prevElapsedTime) : 0;
	//float fElapsedTime = static_cast<float>(elapsedTime) / 1000.f;

	//_prevElapsedTime = GetTickCount64();

	//DoTimer(50, &Room::Tick, 0.05f);
}

void Room::BeginPlay()
{
	// TODO : begin play
}
