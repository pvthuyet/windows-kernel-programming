// FileBackupMon.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include "..\FileBackup\FileBackupCommon.h"
#include <fltUser.h>

#pragma comment(lib, "fltlib")

void HandleMessage(const BYTE* buffer) 
{
	auto msg = (FileBackupPortMessage*)buffer;
	std::wstring filename(msg->FileName, msg->FileNameLength);

	printf("file backed up: %ws\n", filename.c_str());
}

int wmain(int argc, const wchar_t* argv[])
{
	HANDLE hPort = nullptr;
	auto hr = ::FilterConnectCommunicationPort(FILE_BACKUP_PORT, 0, nullptr, 0, nullptr, &hPort);
	if (FAILED(hr)) {
		printf("Error connecting to port (HR=0x%08X)\n", hr);
		return 1;
	}

	BYTE buffer[1 << 12] = { 0 };	// 4 KB
	auto message = (FILTER_MESSAGE_HEADER*)buffer;
	for (;;)
	{
		std::cout << "Wait message from kernel...\n";
		hr = ::FilterGetMessage(hPort, message, sizeof(buffer), nullptr);		
		if (FAILED(hr)) {
			printf("Error receiving message (0x%08X)\n", hr);
			break;
		}
		HandleMessage(buffer + sizeof(FILTER_MESSAGE_HEADER));
	}
	::CloseHandle(hPort);
    std::cout << "File backup mon exit!\n";
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
