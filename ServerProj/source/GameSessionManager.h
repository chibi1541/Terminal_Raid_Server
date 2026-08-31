#pragma once

#include "GameSession.h"

class GameSessionManager
{
public:
	void Add(GameSessionRef gameSession);
	void Remove(GameSessionRef gameSession);
	void Broadcast(SendBufferRef sendBuffer);

private:
	USE_LOCK;
	// GMemory 찐빠남
	// GameSessionManager가 전역 객체라 초기화가 먼저되서
	// 커스텀 자료구조 쓰면 메모리 할당하는 타이밍에 GMemory가 아직 초기화 전이어서 문제가 발생
	set<GameSessionRef> _sessions;

};

extern GameSessionManager GSessionManager;

