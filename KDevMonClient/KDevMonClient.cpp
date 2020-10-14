// KDevMonClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include "..\KDevMon\KDevMonCommon.h"

int Usage() 
{
	printf("Devmon.exe <command> [args]\n");
	printf("Commands:\n");
	printf("\tadd <devicename> (adds a device to monitor)\n");
	printf("\tremove <devicename> (remove device from monitoring)\n");
	printf("\tclear (remove all devices)\n");

	return 0;
}

int Error(const char* text) 
{
	printf("%s (%d)\n", text, ::GetLastError());
	return 1;
}

int wmain(int argc, const wchar_t* argv[])
{
	if (argc < 2) {
		return Usage();
	}

	auto& cmd = argv[1];

	HANDLE hDevice = ::CreateFile(L"\\\\.\\kdevmon", GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hDevice == INVALID_HANDLE_VALUE) {
		return Error("Failed to open device");
	}

	DWORD bytes{0};
	if (::_wcsicmp(cmd, L"add") == 0) 
	{
		if (!::DeviceIoControl(hDevice, IOCTL_DEVMON_ADD_DEVICE, (PVOID)argv[2],
			static_cast<DWORD>(::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &bytes, nullptr))
		{
			return Error("Failed in add device");
		}
		printf("Add device %ws successful.\n", argv[2]);
		return 0;
	}
	else if (::_wcsicmp(cmd, L"remove") == 0)
	{
		if (!::DeviceIoControl(hDevice, IOCTL_DEVMON_REMOVE_DEVICE, (PVOID)argv[2],
			static_cast<DWORD>(::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &bytes, nullptr))
			return Error("Failed in remove device");
		printf("Remove device %ws successful.\n", argv[2]);
		return 0;
	}
	else if (::_wcsicmp(cmd, L"clear") == 0)
	{
		if (!::DeviceIoControl(hDevice, IOCTL_DEVMON_REMOVE_ALL,
			nullptr, 0, nullptr, 0, &bytes, nullptr))
			return Error("Failed in remove all devices");
		printf("Removed all devices successful.\n");
	}
	else
	{
		printf("Unknown command.\n");
		return Usage();
	}
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
