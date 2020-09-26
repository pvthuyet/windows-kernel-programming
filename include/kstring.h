#pragma once

#include <ntddk.h>

class kstring final
{
public:
	explicit kstring(const wchar_t* str = nullptr, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	kstring(const wchar_t* str, ULONG count, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	explicit kstring(PCUNICODE_STRING str, POOL_TYPE pool = PagedPool, ULONG tag = 0);
	kstring(const kstring& other);
	kstring(kstring&& other);
	~kstring();

	kstring& operator= (const kstring& other);
	kstring& operator=(kstring&& other);
	kstring& operator+=(const kstring& other);
	kstring& operator+=(PCWSTR str);
	bool operator==(const kstring& other) const;
	operator const wchar_t* () const;
	const wchar_t operator[](size_t index) const;
	wchar_t& operator[](size_t index);

	const wchar_t* Get() const;
	size_t Length() const;

	kstring ToLower() const;
	kstring& ToLower();

	kstring& Truncate(ULONG length);
	kstring& Append(PCWSTR str, size_t len = 0);

	const wchar_t GetAt(size_t index) const;
	wchar_t& GetAt(size_t index);
	UNICODE_STRING* GetUnicodeString(PUNICODE_STRING) const;

	void Release();

private:
	wchar_t* Allocate(size_t newBufferSize, const wchar_t* src = nullptr, size_t srcLen = 0) const;

private:
	wchar_t* m_str;
	size_t m_Len;
	size_t m_Capacity;
	POOL_TYPE m_Pool;
	ULONG m_Tag;
};

