#pragma once
class BufferWriter
{
public:
	BufferWriter();
	BufferWriter(BYTE* buffer, uint32 size, uint32 pos = 0);
	~BufferWriter();

	BYTE* Buffer() { return _buffer; }
	uint32 Size() { return _size; }
	uint32 WriteSize() { return _pos; }
	uint32 FreeSize() { return _size - _pos; }

	template<typename T>
	T* Reserve();

	template<typename T>
	bool Write(T* src) { return Write(src, sizeof(T)); }
	bool Write(void* src, uint32 len);

	// 아래있는 T&& 받는 버전이 보편 참조라서 lvalue도 함께 처리됨
	//template<typename T>
	//inline BufferWriter& operator<<(const T& src);

	template<typename T>
	inline BufferWriter& operator<<(T&& src);

private:
	BYTE* _buffer = nullptr;
	uint32 _pos = 0;
	uint32 _size = 0;

};

template<typename T>
T* BufferWriter::Reserve()
{
	if(FreeSize() < sizeof(T))
		return nullptr;

	T* ret = reinterpret_cast<T*>(&_buffer[_pos]);
	_pos += sizeof(T);
	return ret;
}

template<typename T>
BufferWriter& BufferWriter::operator<<(T&& src)
{
	using DataType = std::remove_reference_t<T>;
	*reinterpret_cast<DataType*>(&_buffer[_pos]) = std::forward<DataType>(src);
	_pos += sizeof(T);
	return *this;
}

