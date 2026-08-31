#pragma once
#include <vector>
#include "Types.h"

/*-----------------
	FileUtils
------------------*/

class FileUtils
{
public:
	static Vector<BYTE>		ReadFile(const WCHAR* path);
	static String			Convert(string str);
};

// FileUtils.cpp

#include "pch.h"
#include "FileUtils.h"
#include <filesystem>
#include <fstream>

/*-----------------
	FileUtils
------------------*/

// C++ 17에서 추가된 기능, 버전을 올려야함
namespace fs = std::filesystem;

Vector<BYTE> FileUtils::ReadFile(const WCHAR* path)
{
	Vector<BYTE> ret;

	fs::path filePath{ path };

	const uint32 fileSize = static_cast<uint32>(fs::file_size(filePath));
	ret.resize(fileSize);

	basic_ifstream<BYTE> inputStream{ filePath };
	inputStream.read(&ret[0], fileSize);

	return ret;
}

String FileUtils::Convert(string str)
{
	const int32 srcLen = static_cast<int32>(str.size());

	String ret;
	if (srcLen == 0)
		return ret;

	const int32 retLen = ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(&str[0]), srcLen, NULL, 0);
	ret.resize(retLen);
	::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(&str[0]), srcLen, &ret[0], retLen);

	return ret;
}

