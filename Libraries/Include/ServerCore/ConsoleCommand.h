#pragma once
#include <thread>
#include <string>

/*---------------
	ConsoleCommand

	서버 프로세스 콘솔(stdin)에서 명령을 읽어 GCommandRegistry로 넘긴다.
	전용 스레드를 쓰므로 IOCP 워커를 막지 않는다.

	stdin이 없는 환경(서비스/백그라운드 실행)에서는 Start()가 조용히 no-op이 된다.
----------------*/

class ConsoleCommand
{
public:
	~ConsoleCommand();

	bool	Start(const WCHAR* prompt = L"> ");
	void	Stop();

	// 입력 루프가 아직 살아 있는지. stdin이 EOF면 스스로 false가 된다.
	bool	IsRunning() const { return _running.load(); }

private:
	void	InputLoop();

private:
	Atomic<bool>	_started{ false };		// Start/Stop 짝을 맞추는 플래그
	Atomic<bool>	_running{ false };		// 입력 루프 생존 여부
	Atomic<bool>	_loopExited{ false };	// 루프가 스스로 빠져나왔는지 (join 가능)
	std::wstring	_prompt = L"> ";
	std::thread		_thread;
};
