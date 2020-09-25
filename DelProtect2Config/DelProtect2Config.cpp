// DelProtect2Config.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "stdafx.h"
#include "..\DelProtect2\DelProtectCommon.h"
#include <iostream>

int Error(const char* text) {
	printf("%s (%d)\n", text, ::GetLastError());
	return 1;
}

int PrintUsage() {
	printf("Usage: DelProtect2Config <option> [exename]\n");
	printf("\tOption: add, remove or clear\n");
	return 0;
}

int wmain(int argc, const wchar_t* argv[])
{
	if (argc < 2)
	{
		return PrintUsage();
	}

	HANDLE hDevice = ::CreateFileW(USER_FILE_NAME, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr, OPEN_EXISTING, 0, nullptr);

	if (hDevice == INVALID_HANDLE_VALUE)
		return Error("Failed to open handle to device");

	DWORD returned{ 0 };
	BOOL success { FALSE };
	bool badOption = false;
	if (::_wcsicmp(argv[1], L"add") == 0)
	{
		if (argc < 3)
		{
			return PrintUsage();
		}
		success = ::DeviceIoControl(hDevice, IOCTL_DELPROTECT_ADD_EXE,
			(PVOID)argv[2], ((DWORD)::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returned, nullptr);
	}
	else if (::_wcsicmp(argv[1], L"remove") == 0)
	{
		if (argc < 3)
		{
			return PrintUsage();
		}
		success = ::DeviceIoControl(hDevice, IOCTL_DELPROTECT_REMOVE_EXE,
			(PVOID)argv[2], ((DWORD)::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returned, nullptr);
	}
	else if (::_wcsicmp(argv[1], L"clear") == 0)
	{
		success = ::DeviceIoControl(hDevice, IOCTL_DELPROTECT_CLEAR, nullptr, 0, nullptr, 0, &returned, nullptr);
	}
	else
	{
		badOption = true;
		printf("Unknown option.\n");
		PrintUsage();
	}

	if (!badOption)
	{
		if (!success)
		{
			Error("Failed in operation");
		}
		else
		{
			printf("Success.\n");
		}
	}
	::CloseHandle(hDevice);
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
