#pragma once

class Session;

enum class EventType : uint8
{
	Connect,
	Disconnect,
	Accept,
	//PreRecv,
	Recv,
	Send
};

/*--------------
	IocpEvent
---------------*/

class IocpEvent : public OVERLAPPED
{
public:
	// virtual 함수를 선언하게 되면 시작 주소가 가상 함수 테이블을 지정하게 되어
	// OVERLAPPED형으로 시작 주소를 변환하는 과정에 문제가 발생할 수 있으므로 virtual 함수를 선언하면 안됨
	IocpEvent(EventType type);

	void		Init();

public:
	EventType			eventType;
	// IocpEvent에서 IocpObject를 접근
	IocpObjectRef		owner;
};


/*----------------
	ConnectEvent
-----------------*/
class ConnectEvent : public IocpEvent
{
public:
	ConnectEvent() : IocpEvent(EventType::Connect) {}
};

/*----------------
	DisconnectEvent
-----------------*/
class DisconnectEvent : public IocpEvent
{
public:
	DisconnectEvent() : IocpEvent(EventType::Disconnect) {}
};

/*----------------
	AcceptEvent
-----------------*/

class AcceptEvent : public IocpEvent
{
public:
	AcceptEvent() : IocpEvent(EventType::Accept) {}

public:
	SessionRef		session;
};

/*----------------
	RecvEvent
-----------------*/

class RecvEvent : public IocpEvent
{
public:
	RecvEvent() : IocpEvent(EventType::Recv) {}
};

/*----------------
	SendEvent
-----------------*/

class SendEvent : public IocpEvent
{
public:
	SendEvent() : IocpEvent(EventType::Send) {}

public:
	// SendBuffer를 ref형으로 보관하는 건
	// 여러 소켓에 같은 데이터를 보낼 때 전부 보낼때 까지 레퍼런스 카운트를 체크해서
	// 도중에 데이터가 삭제되지 않게하기 위함
	Vector<SendBufferRef> sendBuffers;
};