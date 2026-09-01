#pragma once
#include "AI/Blackboard.h"

class Room;
class GameObject;
class BehaviorTree;

/*-------------
	BtContext

	리프가 바깥을 만지는 유일한 통로.
	리프 자신은 상태를 갖지 않으므로 필요한 것은 전부 여기로 들어온다.
--------------*/

struct BtContext
{
	Room*		room = nullptr;
	GameObject*	self = nullptr;
	Blackboard*	blackboard = nullptr;
	float		deltaTime = 0.0f;
};

/*------------
	BtParams

	캔버스 노드의 "키: 값" 줄들. 로드 시점에만 쓰고 런타임에는 남지 않는다.
-------------*/

class BtParams
{
public:
	void	Add(const string& key, const string& value);
	bool	Has(const string& key) const;

	string	GetString(const string& key, const string& defaultValue = "") const;
	int64	GetInt(const string& key, int64 defaultValue = 0) const;
	float	GetFloat(const string& key, float defaultValue = 0.0f) const;
	bool	GetBool(const string& key, bool defaultValue = false) const;

	const HashMap<string, string>& GetAll() const { return _values; }

private:
	HashMap<string, string> _values;
};

/*----------
	BtLeaf

	★ 공유된다. 몬스터 전원이 같은 인스턴스를 가리킨다. ★

	Execute 가 const 인 것이 이 설계의 핵심이다.
	리프가 자기 멤버에 진행 상태를 쌓으면 컴파일이 안 된다.
	타이머 같은 값은 전부 context 의 블랙보드에 둬야 한다.

	생성자에서 받은 파라미터(슬롯 인덱스, 상수 등)만 멤버로 들고, 그건 로드 후 불변이다.
-----------*/

class BtLeaf
{
public:
	virtual ~BtLeaf() = default;

	virtual BtStatus Execute(BtContext& context) const = 0;

	// Debug 출력용 한 줄 요약. 파라미터가 어떻게 해석됐는지 보여준다.
	virtual std::wstring Describe() const { return L""; }
};
