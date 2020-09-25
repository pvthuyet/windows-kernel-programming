#include "kstring.h"

kstring::kstring(const wchar_t* str, POOL_TYPE pool, ULONG tag) :
	kstring(str, 0, pool, tag)
{	
}

kstring::kstring(const wchar_t* str, ULONG count, POOL_TYPE pool, ULONG tag) :
	m_Pool { pool },
	m_Tag { tag },
	m_Len { 0 },
	m_Capacity { 0 },
	m_str { nullptr }
{
	if (str)
	{
		m_Len = (0 == count) ? static_cast<ULONG>(wcslen(str)) : count;
		m_Capacity = m_Len + 1;
		m_str = Allocate(m_Capacity, str, m_Len);
		if (!m_str)
		{
			ExRaiseStatus(STATUS_NO_MEMORY);
		}
	}
}

kstring::kstring(PCUNICODE_STRING str, POOL_TYPE pool, ULONG tag) :
	m_Pool { pool },
	m_Tag { tag },
	m_Len { 0 },
	m_Capacity { 0 },
	m_str { nullptr }
{
	m_Len = str->Length / sizeof(wchar_t);
	if (m_Len > 0)
	{
		m_Capacity = m_Len + 1;
		m_str = Allocate(m_Capacity, str->Buffer, m_Len);
		if (!m_str)
		{
			ExRaiseStatus(STATUS_NO_MEMORY);
		}
	}
}

kstring::kstring(const kstring& other) :
	m_Pool{ other.m_Pool },
	m_Tag{ other.m_Tag },
	m_Len{ other.m_Len },
	m_Capacity{ other.m_Capacity },
	m_str{ nullptr }
{
	if (other.m_Len > 0)
	{
		m_str = Allocate(other.m_Capacity, other.m_str, other.m_Len);
		if (!m_str)
		{
			ExRaiseStatus(STATUS_NO_MEMORY);
		}
	}
}

kstring::kstring(kstring&& other) : 
	m_Pool{ other.m_Pool },
	m_Tag{ other.m_Tag },
	m_Len{ other.m_Len },
	m_Capacity{ other.m_Capacity },
	m_str{ other.m_str }
{
	// Reset other
	other.m_Len = other.m_Capacity = 0;
	other.m_str = nullptr;
}

kstring::~kstring()
{
	Release();
}

kstring& kstring::operator=(const kstring& other)
{
	if (this != &other)
	{
		Release();
		this->m_Pool = other.m_Pool;
		this->m_Tag = other.m_Tag;
		this->m_Len = other.m_Len;
		this->m_Capacity = other.m_Capacity;
		if (other.m_Len > 0)
		{
			this->m_str = Allocate(other.m_Capacity, other.m_str, other.m_Len);
			if (!this->m_str)
			{
				ExRaiseStatus(STATUS_NO_MEMORY);
			}
		}
	}
	return *this;
}

kstring& kstring::operator=(kstring&& other)
{
	if (this != &other)
	{
		Release();
		this->m_Pool = other.m_Pool;
		this->m_Tag = other.m_Tag;
		this->m_Len = other.m_Len;
		this->m_Capacity = other.m_Capacity;
		this->m_str = other.m_str;
		// Reset other
		other.m_Capacity = other.m_Len = 0;
		other.m_str = nullptr;
	}
	return *this;
}

kstring& kstring::operator+=(const kstring& other)
{
	return Append(other.m_str, other.m_Len);
}

kstring& kstring::operator+=(PCWSTR str)
{
	return Append(str, 0);
}

bool kstring::operator==(const kstring& other) const
{
	if (this != &other)
	{
		if (this->m_Len != other.m_Len)
		{
			return false;
		}

		if (0 == this->m_Len)
		{
			return true;
		}

		return 0 == wcsncmp(this->m_str, other.m_str, this->m_Len);
	}
	return true;
}

kstring::operator const wchar_t* () const
{
	return this->m_str;
}

const wchar_t kstring::operator[](size_t index) const
{
	return GetAt(index);
}

wchar_t& kstring::operator[](size_t index)
{
	return GetAt(index);
}

const wchar_t* kstring::Get() const
{
	return this->m_str;
}

ULONG kstring::Length() const
{
	return this->m_Len;
}

kstring kstring::ToLower() const
{
	kstring tmp(*this);
	_wcslwr_s(tmp.m_str, tmp.m_Len);
	return tmp;
}

kstring& kstring::ToLower()
{
	_wcslwr_s(m_str, m_Len);
	return *this;
}

kstring& kstring::Truncate(ULONG length)
{
	if (m_Len > length)
	{
		m_Len = length;
		m_str[m_Len] = L'\0';
	}
	return *this;
}

kstring& kstring::Append(PCWSTR str, ULONG len)
{
	if (!str)
	{
		return *this;
	}

	if (0 == len)
	{
		len = static_cast<ULONG>(wcslen(str));
	}

	if (len > 0)
	{
		auto newAlloc = false;
		auto newBuffer = m_str;
		auto newLen = m_Len + len;
		auto newCapacity = m_Capacity;

		if (newLen + 1 > m_Capacity)
		{
			newCapacity = newLen + 1;
			newBuffer = Allocate(newCapacity, m_str, m_Len);
			newAlloc = true;
		}

		// copy 
		wcsncat_s(newBuffer, newCapacity, str, len);
		if (newAlloc)
		{
			Release();
			this->m_Len = newLen;
			this->m_Capacity = newCapacity;
			this->m_str = newBuffer;
		}
	}
	return *this;
}

const wchar_t kstring::GetAt(size_t index) const
{
	NT_ASSERT(m_Len > index);
	return m_str[index];
}

wchar_t& kstring::GetAt(size_t index)
{
	NT_ASSERT(m_Len > index);
	return m_str[index];
}

UNICODE_STRING* kstring::GetUnicodeString(PUNICODE_STRING pUnicodeString) const
{
	RtlInitUnicodeString(pUnicodeString, m_str);
	return pUnicodeString;
}

void kstring::Release()
{
	if (m_str)
	{
		ExFreePoolWithTag(m_str, m_Tag);
		m_str = nullptr;
		m_Capacity = m_Len = 0;
	}
}

wchar_t* kstring::Allocate(size_t newBufferSize, const wchar_t* src, size_t srcLen) const
{
	NT_ASSERT(newBufferSize > srcLen);
	auto str = static_cast<wchar_t*>(ExAllocatePoolWithTag(m_Pool, newBufferSize * sizeof(wchar_t), m_Tag));
	if (!str)
	{
		KdPrint(("Failed to allocate kstring of length %lu chars\n", newBufferSize));
		return nullptr;
	}

	if (srcLen > 0)
	{
		NT_ASSERT(nullptr != src);
		wcsncpy_s(str, newBufferSize, src, srcLen);
	}
	return str;
}