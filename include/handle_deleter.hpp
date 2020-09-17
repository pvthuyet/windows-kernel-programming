#pragma once
#include <Windows.h>

namespace fibo
{
	struct CloseHandleDeleter
	{
		void operator()(HANDLE hObj) const
		{
			::CloseHandle(hObj);
		}
	};

	struct DeleteObjectDeleter
	{
		void operator()(HGDIOBJ hObj) const
		{
			::DeleteObject(hObj);
		}
	};

	struct RegCloseKeyDeleter
	{
		void operator()(HKEY hObj) const
		{
			::RegCloseKey(hObj);
		}
	};

	struct CloseFileDeleter
	{
		void operator()(FILE* hObj) const
		{
			::fclose(hObj);
		}
	};

	struct CloseSocketDeleter
	{
		void operator()(SOCKET hObj) const
		{
			::closesocket(hObj);
		}
	};

	struct UnmapViewOfFileDeleter
	{
		void operator()(LPVOID hObj) const
		{
			::UnmapViewOfFile(hObj);
		}
	};

	struct FreeLibraryDeleter
	{
		void operator()(HMODULE hObj) const
		{
			::FreeLibrary(hObj);
		}
	};

	struct FindCloseDeleter
	{
		void operator()(HANDLE hObj) const
		{
			::FindClose(hObj);
		}
	};
}