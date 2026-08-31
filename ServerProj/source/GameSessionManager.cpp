#include "pch.h"
#include "GameSessionManager.h"
#include "GameSession.h"
#include "Room.h"

GameSessionManager GSessionManager;

void GameSessionManager::Add(GameSessionRef gameSession)
{
	WRITE_LOCK;
	_sessions.insert(gameSession);
}

void GameSessionManager::Remove(GameSessionRef gameSession)
{
	WRITE_LOCK;
	_sessions.erase(gameSession);
}

void GameSessionManager::Broadcast(SendBufferRef sendBuffer)
{
	WRITE_LOCK;
	for(GameSessionRef gameSession : _sessions)
	{
		gameSession->Send(sendBuffer);
	}
}
