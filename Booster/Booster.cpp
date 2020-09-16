// Booster.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include "..\PriorityBooster\PriorityBoosterCommon.h"

int Error(const char*);
int main(int argc, const char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: Booster <threadid> <priority>\n";
		return 0;
	}

	HANDLE hDevice = CreateFile(L"\\\\.\\PriorityBooster", 
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		return Error("Failed to open device");
	}

	ThreadData data;
	data.ThreadId = atoi(argv[1]);
	data.Priority = atoi(argv[2]);
	DWORD returned = 0;
	BOOL success = DeviceIoControl(hDevice,
		IOCTL_PRIORITY_BOOSTER_SET_PRIORITY,
		&data, 
		sizeof(data),
		nullptr, 
		0,
		&returned,
		nullptr);

	if (success)
	{
		std::cout << "Priority change successed\n";
	}
	else
	{
		Error("Priority change failed");
	}

	::CloseHandle(hDevice);
}

int Error(const char* message)
{
	printf("%s (error=%d)\n", message, ::GetLastError());
	return 1;
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
