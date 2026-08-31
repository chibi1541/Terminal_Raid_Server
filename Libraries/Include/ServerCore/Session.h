#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAddress.h"
#include "RecvBuffer.h"

/*--------------
	Session
---------------*/
class Session : public IocpObject
{
	friend class IocpCore;
	friend class Listener;
	friend class Service;

	enum{ BUFFER_SIZE = 0x10000 };


public:
	Session();
	virtual ~Session();

public:
	
	void				Disconnect(const WCHAR* cause);
	bool				Connect();
	void				Send(SendBufferRef buffer);

	shared_ptr<Service>	GetService() { return _service.lock(); }
	void				SetService(shared_ptr<Service> service) { _service = service; }

public:
	/* 정보 관련 */
	void		SetNetAddress(NetAddress address) { _netAddress = address; }
	NetAddress	GetAddress() { return _netAddress; }
	SOCKET		GetSocket() { return _socket; }
	bool		IsConnected() {return _connected;}
	SessionRef  GetSessionRef() {return static_pointer_cast<Session>(shared_from_this()); }

private:
	/* 인터페이스 구현 */
	virtual HANDLE		GetHandle() override;
	virtual void		Dispatch(IocpEvent* iocpEvent, int32 numOfBytes = 0) override;

private:
	bool RegisterConnect();
	bool RegisterDisconnect();
	void RegisterRecv();
	void RegisterSend();

	void ProcessConnect();
	void ProcessDisconnect();
	void ProcessRecv(int32 numOfBytes);
	void ProcessSend(int32 numOfBytes);

	void HandleError(int32 errorCode);


protected:
	// 필요한 상황에 컨텐츠 코드에서 오버라이드 할 함수
	virtual void OnConnected() {}
	virtual int32 OnRecv(BYTE* buffer, int32 len) {return len;}
	virtual void OnSend(int32 len) {}
	virtual void OnDisconnected() {}

private:
	std::weak_ptr<Service>	_service;
	SOCKET			_socket = INVALID_SOCKET;
	NetAddress		_netAddress = {};
	Atomic<bool>	_connected = false;

	RecvBuffer		_recvBuffer;

	Queue<SendBufferRef> _sendQueue;
	Atomic<bool>		_sendRegistered = false;

private:
	USE_LOCK;

private:
	RecvEvent		_recvEvent;
	ConnectEvent	_connectEvent;
	DisconnectEvent	_disconnectEvent;
	SendEvent		_sendEvent;
};

/*------------------
	PacketSession
--------------------*/

struct PacketHeader
{
	uint16 id;
	uint16 size;
};

class PacketSession : public Session
{
public:
	PacketSession();
	virtual ~PacketSession();

protected:
	virtual int32		OnRecv(BYTE* buffer, int32 len) sealed;
	virtual void		OnRecvPacket(BYTE* buffer, int32 len) abstract;
};

