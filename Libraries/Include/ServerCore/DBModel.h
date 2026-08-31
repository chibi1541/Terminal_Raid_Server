#pragma once
#include "Types.h"

// namespace를 선언하는 매크로를 CoreMacro에 추가
NAMESPACE_BEGIN(DBModel)

USING_SHARED_PTR(Column);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Procedure);

/*-------------
	DataType
--------------*/

// SQL Server 기준으로 각 오브젝트 타입
// SQL Server를 사용하지 않으면 값 수정이 필요함
enum class DataType
{
	None = 0,
	TinyInt = 48,
	SmallInt = 52,
	Int = 56,
	Real = 59,
	DateTime = 61,
	Float = 62,
	Bit = 104,
	Numeric = 108,
	BigInt = 127,
	VarBinary = 165,
	Varchar = 167,
	Binary = 173,
	NVarChar = 231,
};

/*-------------
	Column
--------------*/

class Column
{
public:
	String				CreateText();

public:
	String				_name;
	int32				_columnId = 0; // DB 한정 정보

	// Type 정보 
	DataType			_type = DataType::None;
	String				_typeText;

	int32				_maxLength = 0;
	bool				_nullable = false;

	// identity 정보
	bool				_identity = false;
	int64				_seedValue = 0;
	int64				_incrementValue = 0;

	// 기본 값에 대한 정보
	String				_default;
	String				_defaultConstraintName; // DB 한정 정보
};

/*-----------
	Index
------------*/

enum class IndexType
{
	Clustered = 1,
	NonClustered = 2
};

class Index
{
public:
	String				GetUniqueName();
	String				CreateName(const String& tableName);
	String				GetTypeText();
	String				GetKeyText();
	String				CreateColumnsText();
	bool				DependsOn(const String& columnName);

public:
	String				_name; // DB 한정 정보
	int32				_indexId = 0; // DB 한정 정보
	IndexType			_type = IndexType::NonClustered;
	bool				_primaryKey = false;
	bool				_uniqueConstraint = false;
	Vector<ColumnRef>	_columns;
};

/*-----------
	Table
------------*/

class Table
{
public:
	ColumnRef			FindColumn(const String& columnName);

public:
	int32				_objectId = 0; // DB 한정 정보
	String				_name;
	Vector<ColumnRef>	_columns;
	Vector<IndexRef>	_indexes;
};

/*----------------
	Procedures
-----------------*/

struct Param
{
	String				_name;
	String				_type;
};

class Procedure
{
public:
	String				GenerateCreateQuery();
	String				GenerateAlterQuery();
	String				GenerateParamString();

public:
	String				_name;
	String				_fullBody; // DB 한정 정보
	String				_body; // XML 한정 정보
	Vector<Param>		_parameters;  // XML 한정 정보
};

/*-------------
	Helpers
--------------*/

class Helpers
{
public:
	static String		Format(const WCHAR* format, ...);
	static String		DataType2String(DataType type);
	static String		RemoveWhiteSpace(const String& str);
	static DataType		String2DataType(const WCHAR* str, OUT int32& maxLen);
};

NAMESPACE_END