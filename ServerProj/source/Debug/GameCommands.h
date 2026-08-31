#pragma once

/*-----------------
	GameCommands

	게임 상태를 만지는 콘솔 / Admin 명령을 GCommandRegistry에 등록한다.
	전부 CommandRunMode::GameThread로 등록하므로 룸의 JobQueue 안에서 실행된다.
	=> 패킷 핸들러와 똑같은 직렬화 보장을 받는다. 락이 필요 없다.

	main()에서 GCommandRegistry->SetGameJobQueue(GRoom)을 부른 뒤에 등록할 것.
------------------*/

class GameCommands
{
public:
	static void Register();
};
