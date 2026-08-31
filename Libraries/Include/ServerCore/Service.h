#pragma once

#include "NetAddress.h"
#include "Session.h"
#include <functional>

enum class ServiceType : uint8
{
	Server,
	Client,
};

/*-------------
	Service
--------------*/

using SessionFactory = function<SessionRef(void)>;

class Service : public enable_shared_from_this<Service>
{
public:
	Service(ServiceType type, NetAddress address, IocpCoreRef core, SessionFactory sessionFactory, int32 maxSessionCount = 1);
	virtual ~Service();

	virtual bool Start() abstract;
	bool CanStart() {return _sessionFactory != nullptr;}

	virtual void CloseService();
	void SetSessionFactory(SessionFactory func) {_sessionFactory = func;}

	// 내부적으로 Session 생성 후에 Iocp에 등록하는 과정이 함께 있으므로
	// CreateSession보다는 CreateAndRegisterSession이 좋을 듯?
	SessionRef CreateSession();
	void AddSession(SessionRef session);
	void ReleaseSession(SessionRef session);
	int32 GetCurrentSessionCount() {return _sessionCount;}
	int32 GetMaxSessionCount() {return _maxSessionCount;}

	/* Monitoring : snapshot copy of the live session set */
	void CollectSessions(OUT Vector<SessionRef>& sessions);

	void Broadcast(SendBufferRef sendBuffer);

public:
	ServiceType GetServiceType() {return _type;}
	NetAddress GetAddress() {return _netAddress;}
	// 굳에 참조 카운터를 늘리지 않기 위해 참조형으로 반환
	IocpCoreRef& GetIocpCore() {return _iocpCore;}


protected:
	USE_LOCK;
	ServiceType _type;
	NetAddress _netAddress = {};
	IocpCoreRef _iocpCore;

	// Session의 Id 같은 걸로 구분해도 좋지만 지금은 포인터 주소로만 구분
	Set<SessionRef> _sessions;
	int32 _sessionCount = 0;
	int32 _maxSessionCount = 0;
	SessionFactory _sessionFactory;
};


/*-----------------
	ClientService
------------------*/

class ClientService : public Service
{
public:
	ClientService(NetAddress targetAddress, IocpCoreRef core, SessionFactory sessionFactory, int32 maxSessionCount = 1);
	virtual ~ClientService() {};

	virtual bool Start() override;
};
/*-----------------

	ServerService
------------------*/

class ServerService : public Service
{
public:
	ServerService(NetAddress address, IocpCoreRef core, SessionFactory sessionFactory, int32 maxSessionCount = 1);
	virtual ~ServerService() {};

	virtual bool Start() override;
	virtual void CloseService() override;

private:
	// 서버 역할이므로 Accept를 수행할 리스너가 필요
	ListenerRef _listener = nullptr;
};