#pragma once
#include "Protocol/Struct.pb.h"
#include "Game/Bounds.h"
#include "Game/NavGrid.h"	// TilePos
#include "Shared/MovementMath.h"	// 좌표 눈금 · 이동 적분식의 유일한 정의 (클라와 공유)

class Room;

/*---------------
	이동 (Movement)

	위치는 두 겹이다.
	- GameObject::_pos       : 정수 셀. 충돌 / 쿼드트리 / 길찾기 / 직렬화가 보는 값.
	- MovementComponent::fp* : 1/256 셀 고정소수점. 매 틱 등속 적분이 여기 쌓인다.
	  셀은 항상 fp >> POS_SHIFT 로 파생하므로 두 값이 어긋나지 않는다.
	  float를 안 쓰는 이유는 JpsPathFinder / OverlapsCircle 와 같다 - 결정성.
----------------*/

// 값의 실체는 Shared/MovementMath.h 에 있다. 여기서는 기존 이름만 그대로 재노출한다
// (클라와 서버가 같은 헤더를 보므로 자동으로 일치한다).
constexpr int32 POS_SHIFT = MoveMath::POS_SHIFT;
constexpr int32 POS_SCALE = MoveMath::POS_SCALE;					// 256 : 1셀 = 256 서브유닛
constexpr int32 DEFAULT_MOVE_SPEED_CELLS = MoveMath::DEFAULT_MOVE_SPEED_CELLS;	// 기본 이동 속도 (셀/초)

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

	// 임의 각도 속도 벡터(서브유닛/초). 0 이 아니면 UpdateMovement 가 8방향 dir 대신 이걸로 적분한다.
	// 투사체가 마우스 조준 방향으로 이 값을 받는다. 플레이어/몬스터는 0 (dir 기반).
	int32					velSubX = 0;
	int32					velSubY = 0;

	// AI 가 채우는 타일 경로. 플레이어 방향 이동은 비운 채로 둔다.
	Vector<TilePos>			path;
	int32					pathIndex = 0;

	uint32					lastProcessedInputSeq = 0;	// 클라 예측 ack 훅
	bool					dirty = false;				// 이번 틱에 복제할 값(셀/dir/state)이 바뀜

	// --- 클라 이동 입력 검증(anti-cheat) ---
	// 방향-홀드 모델에서 한 방향이 유지된 구간을, 클라가 주장하는 시간(clientTimeMs 차이)과
	// 서버가 실측한 패킷 도착 간격으로 대조한다. 클라가 시간을 부풀리면 여기서 잡힌다.
	uint32					lastInputClientTimeMs = 0;	// 마지막 C_MOVE 가 실어온 클라 단조 ms
	uint64					lastInputWallMs = 0;		// 그 C_MOVE 를 서버가 처리한 시각(GetTickCount64)
	int64					moveTimeCreditMs = 0;		// (클라 주장 - 서버 허용) 누적. 양수로 계속 쌓이면 어뷰징
	bool					hasInputTimeBase = false;	// 첫 입력 전에는 대조 기준이 없다

	// 재조정(reconciliation) 기준점. 마지막으로 처리한 입력 순간(lastInputClientTimeMs)의
	// 권위 위치. 다음 입력이 오면 여기서부터 heldMs 만큼 정확히 다시 적분한다
	// (매 틱 free-run 이 벌려 놓은 잔차를 이 시점에 되감아 없앤다).
	int32					anchorFpX = 0;
	int32					anchorFpY = 0;

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
	// 서브유닛 고정소수점 위치(1셀=256). 이동 복제(S_MOVE_ACK / MoveInfo)가 셀 대신 이 값을
	// 실어 보내야 클라 재조정/보간이 셀 내부 위치를 잃지 않는다.
	int32						GetFixedX() const	{ return _move.fpX; }
	int32						GetFixedY() const	{ return _move.fpY; }
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
	// 서브셀 정밀 위치를 직접 놓는다 (투사체 스폰). 셀은 fp >> POS_SHIFT 로 파생.
	void SetFixedPos(int32 fpX, int32 fpY)
	{
		_move.fpX = fpX;
		_move.fpY = fpY;
		SyncCellFromFixed();
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

	// 벽 충돌 박스. "셀" 단위 (타일 아님), 위치가 중심. 스프라이트 전체를 덮는다.
	// Room::IsActorBoxBlocked / 클라 예측이 이 값으로 MoveMath::BoxBlockedCells 를 부르고,
	// 길찾기 NavGrid(Level::GetNavGridForCollisionBox)도 이 박스로 장애물을 팽창시킨다 - 이동/길찾기 단일 기준.
	// 기본 1x1 = 선 셀 한 칸 (기존 동작).
	int32 GetCollisionCellsWide() const	{ return _collisionCellsWide; }
	int32 GetCollisionCellsHigh() const	{ return _collisionCellsHigh; }
	void SetCollisionBox(int32 cellsWide, int32 cellsHigh)
	{
		_collisionCellsWide = (cellsWide > 0) ? cellsWide : 1;
		_collisionCellsHigh = (cellsHigh > 0) ? cellsHigh : 1;
	}

	// hp는 항상 [0, maxHp] 안으로 잘린다. 음수 체력이나 과회복이 새어나가지 않게.
	void SetHp(int32 hp);
	void SetMaxHp(int32 maxHp);

	// 공격력 (Character/MonsterData 에서 옴). 몬스터 AI 공격 / 향후 근접 판정이 쓴다.
	int32 GetAttackPower() const			{ return _attackPower; }
	void  SetAttackPower(int32 power)		{ _attackPower = (power >= 0) ? power : 0; }

	// 이동 속도를 "셀/초" 로 지정한다. 내부(_move.speed)는 서브유닛/초 (1셀=256).
	// 0 이면 MovementComponent::EffectiveSpeed 의 기본값(DEFAULT_MOVE_SPEED)으로 굴러간다.
	void  SetMoveSpeedCells(int32 cellsPerSec)
	{
		_move.speed = (cellsPerSec > 0) ? cellsPerSec * POS_SCALE : 0;
	}

	// hp 를 깎는다. 브로드캐스트는 하지 않는 순수 상태 변경 - Room::DealDamage 가 그 몫이다.
	// damage <= 0 이거나 이미 죽었으면 아무 일도 안 하고 false. 이 호출로 사망했으면(hp==0) true.
	bool ApplyDamage(int32 damage);

	// 피격 경직. 이 틱까지는 이동 적분 / AI 틱 / 입력을 무시한다. Room::DealDamage 가 세팅.
	uint64 GetStunUntilTick() const				{ return _stunUntilTick; }
	void   SetStunUntilTick(uint64 tick)		{ _stunUntilTick = tick; }
	bool   IsStunned(uint64 currentTick) const	{ return currentTick < _stunUntilTick; }

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
	int32					_attackPower = 0;
	int32					_collisionCellsWide = 1;
	int32					_collisionCellsHigh = 1;
	uint64					_stunUntilTick = 0;

	weak_ptr<Room>			_room;
};

using GameObjectRef = shared_ptr<GameObject>;
