#pragma once

/*--------------
	SendBuffer
---------------*/
class SendBuffer
{
public:
	SendBuffer(SendBufferChunkRef owner, BYTE* buffer, int32 allocSize);
	~SendBuffer();

public:
	BYTE* Buffer() {return _buffer; }
	uint32 AllocSize() {return _allocSize;}
	uint32 WriteSize() {return _writeSize;}
	void Close(uint32 writeSize);

private:
	BYTE* _buffer;
	uint32 _allocSize;
	uint32 _writeSize;
	SendBufferChunkRef _owner;
};

/*--------------------
	SendBufferChunk
---------------------*/
class SendBufferChunk : public enable_shared_from_this<SendBufferChunk>
{
	enum 
	{
		SENDBUFFER_CHUNK_SIZE = 6000
	};

public:
	SendBufferChunk();
	~SendBufferChunk();

	void			Reset();
	SendBufferRef	Open(uint32 size);
	void			Close(uint32 size);

	BYTE*			Buffer() {return &_buffer[_usedSize]; }
	uint32			FreeSize() { return static_cast<uint32>(_buffer.size()) - _usedSize;}
	bool			IsOpen() { return _open; }

private:
	Array<BYTE, SENDBUFFER_CHUNK_SIZE> _buffer = {};
	bool _open = false;
	uint32 _usedSize = 0;

};

/*------------------------
	SendBufferManager
-------------------------*/
class SendBufferManager
{
public:
	~SendBufferManager();

	SendBufferRef Open(uint32 size);

	/* Monitoring */
	int32 GetChunkCount()
	{
		READ_LOCK;
		return static_cast<int32>(_sendBufferChunks.size());
	}

private:
	SendBufferChunkRef Pop();
	void Push(SendBufferChunkRef buffer);
	// Chunk의 소멸자에 연결해서 pool에 돌아가도록 하는 함수
	static void PushGlobal(SendBufferChunk* buffer);

private:
	USE_LOCK;
	Vector<SendBufferChunkRef> _sendBufferChunks;

	// While shutting down, chunks are freed instead of recycled.
	// (pushing back into _sendBufferChunks while it is being destroyed loops forever)
	static Atomic<bool> s_shuttingDown;
};

