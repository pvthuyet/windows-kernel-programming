// ProcessProtectClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"
#include "..\ProcessProtect\ProcessProtectCommon.h"

int Error(const char* msg) 
{
	printf_s("%s (Error: %d)\n", msg, ::GetLastError());
	return 1;
}

int PrintUsage() 
{
	printf_s("Protect [add | remove | clear] [pid] ...\n");
	return 0;
}

std::vector<DWORD> ParsePid(const WCHAR* buffer[], int count)
{
	std::vector<DWORD> pids;
	for (int i = 0; i < count; ++i)
	{
		pids.push_back(_wtoi(buffer[i]));
	}
	return pids;
}

int wmain(int argc, const wchar_t* argv[])
{
	if (argc < 2)
	{
		return PrintUsage();
	}
	enum class Options
	{
		Unknow,
		Add,
		Remove,
		Clear
	};
	Options option = Options::Unknow;
	if (0 == ::_wcsicmp(argv[1], L"add"))
	{
		option = Options::Add;
	}
	else if (0 == ::_wcsicmp(argv[1], L"remove"))
	{
		option = Options::Remove;
	}
	else if (0 == ::_wcsicmp(argv[1], L"clear"))
	{
		option = Options::Clear;
	}
	else
	{
		printf_s("Unknow option\n");
		return PrintUsage();
	}

	HANDLE hFile = ::CreateFile(L"\\\\.\\" PROCESS_PROTECT_NAME, GENERIC_WRITE|GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		return Error("Failed to open device");
	}

	std::vector<DWORD> pids;
	BOOL success = FALSE;
	DWORD bytes {0};
	switch (option)
	{
	case Options::Add:
		pids = ParsePid(argv + 2, argc - 2);
		success = ::DeviceIoControl(hFile, 
			IOCTL_PROCESS_PROTECT_BY_PID, 
			pids.data(), 
			static_cast<DWORD>(pids.size() * sizeof(DWORD)),
			nullptr,
			0,
			&bytes,
			nullptr);
		break;

	case Options::Remove:
		pids = ParsePid(argv + 2, argc - 2);
		success = ::DeviceIoControl(hFile,
			IOCTL_PROCESS_UNPROTECT_BY_PID,
			pids.data(),
			static_cast<DWORD>(pids.size() * sizeof(DWORD)),
			nullptr,
			0,
			&bytes,
			nullptr);
		break;

	case Options::Clear:
		success = ::DeviceIoControl(hFile, 
			IOCTL_PROCESS_PROTECT_CLEAR,
			nullptr, 
			0, 
			nullptr, 
			0, 
			&bytes, 
			nullptr);
		break;

	default:
		break;
	}

	if (!success)
	{
		Error("Failed in device control");
	}
	else
	{
		printf_s("Operation successed\n");
	}

	::CloseHandle(hFile);
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
