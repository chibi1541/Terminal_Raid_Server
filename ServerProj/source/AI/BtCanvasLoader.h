#pragma once

class BehaviorTree;

/*------------------
	BtCanvasLoader

	Obsidian 캔버스(.canvas, JSON)를 읽어 BehaviorTree 를 채운다.

	클라의 AnimStateMachineLoader 와 표기 규약을 공유한다.
	마크다운 장식(# - *) 제거, 첫 줄은 이름 + [태그], 이후 줄은 "키: 값".

	노드
	  "# Sequence [root]"       첫 토큰이 노드 타입. 루트는 [root] 태그, 정확히 하나.
	  "# Selector 추격 판단"    첫 토큰 뒤의 텍스트는 사람용 라벨이라 무시한다.
	  "# Wait"                  리프. 레지스트리 키가 곧 타입 이름.
	  "duration: 1.5"           리프 파라미터.

	  예약 이름 : Blackboard    키를 미리 선언하는 노드. 트리에는 들어가지 않는다.
	                            "- targetId: 0" 처럼 적고, 값 표기로 타입이 정해진다.

	엣지
	  fromNode 가 부모, toNode 가 자식.
	  라벨의 [N] 은 자식 순서 오버라이드.

	자식 순서
	  정렬 키는 (N, 중심 x, 노드 id).
	  [N] 이 붙은 자식이 N 오름차순으로 앞에 오고, 안 붙은 자식이 뒤에서 x 오름차순.
	  동률은 노드 id 로 가른다.

	  마지막 키가 필요한 이유 - Obsidian 은 팬/줌만 해도 파일을 다시 쓰면서
	  nodes/edges 배열 순서를 뒤섞는다. 배열 순서에 의존하면 실행 결과가 흔들린다.

	로드 시점 검증
	  [root] 가 정확히 하나 / 부모가 둘 이상인 노드 / 사이클 / 루트에서 못 닿는 고아 /
	  컴포짓 자식 수 / 리프 이름이 레지스트리에 있는지 / 참조한 블랙보드 키가 선언됐는지.
	  하나라도 걸리면 무엇이 어디서 틀렸는지 로그에 남기고 실패시킨다.
-------------------*/

class BtCanvasLoader
{
public:
	// 실패하면 false. outTree 는 건드리지 않는다(기존 트리를 날리지 않기 위함).
	static bool LoadFromFile(const WCHAR* path, const string& name, OUT BehaviorTree& outTree);

private:
	BtCanvasLoader() = delete;
};
