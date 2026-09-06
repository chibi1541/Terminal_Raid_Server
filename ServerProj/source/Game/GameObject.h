#pragma once
#include "Protocol/Struct.pb.h"
#include "Game/Bounds.h"
#include "Game/NavGrid.h"	// TilePos

class Room;

/*---------------
	이동 (Movement)

	위치는 두 겹이다.
	- GameObject::_pos       : 정수 셀. 충돌 / 쿼드트리 / 길찾기 / 직렬화가 보는 값.
	- MovementComponent::fp* : 1/256 셀 고정소수점. 매 틱 등속 적분이 여기 쌓인다.
	  셀은 항상 fp >> POS_SHIFT 로 파생하므로 두 값이 어긋나지 않는다.
	  float를 안 쓰는 이유는 JpsPathFinder / OverlapsCircle 와 같다 - 결정성.
----------------*/

constexpr int32 POS_SHIFT = 8;
constexpr int32 POS_SCALE = 1 << POS_SHIFT;			// 256 : 1셀 = 256 서브유닛
constexpr int32 DEFAULT_MOVE_SPEED_CELLS = 6;		// 기본 이동 속도 (셀/초)

enum class MoveState : uint8
{
	Idle,
	Moving,
};

struct MovementComponent
{
	int32					fpX = 0;
	int32					fpY = 0;
	int32					speed = 0;					// 서브유닛/초. 0 이면 기본값.
	Protocol::DirectionType	dir = Protocol::DIR_NONE;
	MoveState				state = MoveState::Idle;

	// AI 가 채우는 타일 경로. 플레이어 방향 이동은 비운 채로 둔다.
	Vector<TilePos>			path;
	int32					pathIndex = 0;

	uint32					lastProcessedInputSeq = 0;	// 클라 예측 ack 훅
	bool					dirty = false;				// 이번 틱에 복제할 값(셀/dir/state)이 바뀜

	int32 EffectiveSpeed() const
	{
		return speed > 0 ? speed : DEFAULT_MOVE_SPEED_CELLS * POS_SCALE;
	}

	bool HasPath() const { return pathIndex < static_cast<int32>(path.size()); }

	void ClearPath() { path.clear(); pathIndex = 0; }
};

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
	// 셀을 직접 놓는다 (스폰 / 텔레포트). 고정소수점을 새 셀 중심으로 같이 시드해서
	// 다음 틱 적분이 어긋난 상태에서 시작하지 않게 한다.
	void SetPos(const Protocol::Vector2& pos)	{ SetPos(pos.x(), pos.y()); }
	void SetPos(int32 x, int32 y)
	{
		_pos.set_x(x);
		_pos.set_y(y);
		_move.fpX = x * POS_SCALE + POS_SCALE / 2;
		_move.fpY = y * POS_SCALE + POS_SCALE / 2;
	}
	void SetRoom(shared_ptr<Room> room)			{ _room = room; }

	MovementComponent&			Movement()			{ return _move; }
	const MovementComponent&	Movement() const	{ return _move; }

	// 고정소수점 위치에서 셀 좌표를 다시 뽑는다. 적분 루프가 매 틱 부른다.
	// SetPos 는 fp 를 셀 중심으로 되돌리므로 이동 중에는 쓰면 안 된다.
	void SyncCellFromFixed();
	// 반지름 0 = 자기가 선 셀 한 칸만 차지한다.
	void SetRadius(int32 radius)				{ _radius = (radius >= 0) ? radius : 0; }
	void ClearRoom()							{ _room.reset(); }

	// 길찾기용 풋프린트. "맵 타일" 단위 (Level::_tileSize 가 정의하는 그 타일).
	// 원형 충돌/쿼드트리에 쓰는 _radius 와는 별개 - 길찾기 샘플링 단위만 바꾼다.
	// 기본 1x1 = Level 이 원래 굽는 기본 NavGrid 그대로.
	int32 GetFootprintTilesWide() const	{ return _footprintTilesWide; }
	int32 GetFootprintTilesHigh() const	{ return _footprintTilesHigh; }
	void SetFootprint(int32 tilesWide, int32 tilesHigh)
	{
		_footprintTilesWide = (tilesWide > 0) ? tilesWide : 1;
		_footprintTilesHigh = (tilesHigh > 0) ? tilesHigh : 1;
	}

	// hp는 항상 [0, maxHp] 안으로 잘린다. 음수 체력이나 과회복이 새어나가지 않게.
	void SetHp(int32 hp);
	void SetMaxHp(int32 maxHp);

	// hp 를 깎는다. 브로드캐스트는 하지 않는 순수 상태 변경 - Room::DealDamage 가 그 몫이다.
	// damage <= 0 이거나 이미 죽었으면 아무 일도 안 하고 false. 이 호출로 사망했으면(hp==0) true.
	bool ApplyDamage(int32 damage);

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
	MovementComponent		_move;
	int32					_radius = 0;
	int32					_hp = 100;
	int32					_maxHp = 100;
	int32					_footprintTilesWide = 1;
	int32					_footprintTilesHigh = 1;

	weak_ptr<Room>			_room;
};

using GameObjectRef = shared_ptr<GameObject>;
