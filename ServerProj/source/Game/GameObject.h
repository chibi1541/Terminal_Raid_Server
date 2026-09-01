#pragma once
#include "Protocol/Struct.pb.h"
#include "Game/Bounds.h"

class Room;

/*---------------
	GameObject

	월드에 존재하는 모든 개체의 공통 베이스.
	Protocol::ObjectInfo / Protocol::CreatureState 와 1:1로 대응한다.
	몬스터는 이 클래스를 상속해서 붙는다.

	모든 상태는 룸의 JobQueue 안에서만 읽고 쓴다. 따로 락을 걸지 않는다.
----------------*/

class GameObject : public enable_shared_from_this<GameObject>
{
public:
	GameObject() = default;
	virtual ~GameObject() = default;

	// ObjectInfo 직렬화를 한 곳에 모은다.
	// 파생 클래스는 super를 부른 뒤 자기 전용 필드만 덧칠한다.
	virtual void FillObjectInfo(Protocol::ObjectInfo* info);

	bool IsAlive() const { return _hp > 0; }

public:
	uint64						GetObjId() const	{ return _objectId; }
	Protocol::ObjectType		GetObjType() const	{ return _objectType; }
	const Protocol::Vector2&	GetPos() const		{ return _pos; }
	int32						GetPosX() const		{ return _pos.x(); }
	int32						GetPosY() const		{ return _pos.y(); }
	int32						GetRadius() const	{ return _radius; }
	int32						GetHp() const		{ return _hp; }
	int32						GetMaxHp() const	{ return _maxHp; }
	// 잠근 결과를 돌려준다. 룸에 없으면 nullptr.
	shared_ptr<Room>			GetRoom() const		{ return _room.lock(); }

	void SetObjId(uint64 objectId)				{ _objectId = objectId; }
	void SetPos(const Protocol::Vector2& pos)	{ _pos.CopyFrom(pos); }
	void SetPos(int32 x, int32 y)				{ _pos.set_x(x); _pos.set_y(y); }
	void SetRoom(shared_ptr<Room> room)			{ _room = room; }
	// 반지름 0 = 자기가 선 셀 한 칸만 차지한다.
	void SetRadius(int32 radius)				{ _radius = (radius >= 0) ? radius : 0; }
	void ClearRoom()							{ _room.reset(); }

	// hp는 항상 [0, maxHp] 안으로 잘린다. 음수 체력이나 과회복이 새어나가지 않게.
	void SetHp(int32 hp);
	void SetMaxHp(int32 maxHp);

	// 쿼드트리 삽입에 쓰는 경계. 원의 바운딩 박스다.
	Bounds GetBounds() const;

	// 원 대 원 겹침. 정수 제곱 거리로 비교하므로 부동소수점이 끼지 않는다.
	bool OverlapsCircle(int32 centerX, int32 centerY, int32 radius) const;

protected:
	// 개체 타입은 각 파생 클래스가 생성자에서 한 번만 정한다.
	void SetObjType(Protocol::ObjectType objectType) { _objectType = objectType; }

private:
	uint64					_objectId = 0;
	Protocol::ObjectType	_objectType = Protocol::OBJECT_NONE;
	Protocol::Vector2		_pos;
	int32					_radius = 0;
	int32					_hp = 100;
	int32					_maxHp = 100;

	weak_ptr<Room>			_room;
};

using GameObjectRef = shared_ptr<GameObject>;
