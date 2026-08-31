#pragma once
#include "NetAddress.h"

/*--------------------
	SocketUtils
---------------------*/

class SocketUtils
{
public:
	// 비동기 소켓 함수의 포인터
	// 이런 방식으로 작동하는 이유는 호출하려는 소켓의 프로토콜(TCP, UDP 등...)에 따라 호출하는 함수가 달라서
	// 예전에는 커넥트 스택 중간에 끼어들수 있는 LSP라는 애들이 있어서(VPN 같은 애들) 런타임이 아니면 호출할 함수를 특정하지 못했는데
	// 현대 Windows(8이후)에서는 WFP라는 방식으로 바뀌어서 Connect 호출까지는 호출 함수를 빌드 타임에 특정이 가능하고 함
	// 그럼에도 이전 버전들과의 호환성을 위해서 이런 방식을 유지한다고...
	static LPFN_CONNECTEX		ConnectEx;
	static LPFN_DISCONNECTEX	DisconnectEx;
	static LPFN_ACCEPTEX		AcceptEx;

public:
	static void Init();
	static void Clear();

	static bool BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* fn);
	static SOCKET CreateSocket();

	static bool SetLinger(SOCKET socket, uint16 onoff, uint16 linger);
	static bool SetReuseAddress(SOCKET socket, bool flag);
	static bool SetRecvBufferSize(SOCKET socket, int32 size);
	static bool SetSendBufferSize(SOCKET socket, int32 size);
	static bool SetTcpNoDelay(SOCKET socket, bool flag);
	static bool SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket);

	static bool Bind(SOCKET socket, NetAddress netAddr);
	static bool BindAnyAddress(SOCKET socket, uint16 port);
	static bool Listen(SOCKET socket, int32 backlog = SOMAXCONN);
	static void Close(SOCKET& socket);

};

template<typename T>
static inline bool SetSockOpt(SOCKET socket, int32 level, int32 optName, T optVal)
{
	return SOCKET_ERROR != ::setsockopt(socket, level, optName, reinterpret_cast<char*>(&optVal), sizeof(T));
}