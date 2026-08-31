#pragma once


class RecvBuffer
{
	enum {BUFFER_COUNT = 10};

public:	
	RecvBuffer(int32 bufferSize);
	~RecvBuffer();

public:
	void Clean();
	// index를 최신화하는 함수
	bool OnWrite(int32 numOfBytes);
	bool OnRead(int32 numOfBytes);

	BYTE* WritePos() {return &_buffer[_writeIndex]; }
	BYTE* ReadPos() {return &_buffer[_readIndex]; }
	int32 DataSize() {return _writeIndex - _readIndex; }
	int32 FreeSize() {return _capacity - _writeIndex; }

private:

	int32 _capacity;
	int32 _bufferSize;
	int32 _writeIndex;
	int32 _readIndex;
	Vector<BYTE> _buffer;

};

