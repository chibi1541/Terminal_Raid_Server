#include "pch.h"
#include "AI/BtLeaf.h"

void BtParams::Add(const string& key, const string& value)
{
	_values[key] = value;
}

bool BtParams::Has(const string& key) const
{
	return _values.find(key) != _values.end();
}

string BtParams::GetString(const string& key, const string& defaultValue) const
{
	auto findIt = _values.find(key);
	if (findIt == _values.end())
		return defaultValue;

	return findIt->second;
}

int64 BtParams::GetInt(const string& key, int64 defaultValue) const
{
	auto findIt = _values.find(key);
	if (findIt == _values.end())
		return defaultValue;

	return ::_atoi64(findIt->second.c_str());
}

float BtParams::GetFloat(const string& key, float defaultValue) const
{
	auto findIt = _values.find(key);
	if (findIt == _values.end())
		return defaultValue;

	return static_cast<float>(::atof(findIt->second.c_str()));
}

bool BtParams::GetBool(const string& key, bool defaultValue) const
{
	auto findIt = _values.find(key);
	if (findIt == _values.end())
		return defaultValue;

	return ::_stricmp(findIt->second.c_str(), "true") == 0
		|| ::_stricmp(findIt->second.c_str(), "1") == 0;
}
