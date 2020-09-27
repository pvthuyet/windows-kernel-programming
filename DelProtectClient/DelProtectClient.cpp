// DelProtectClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"

void HandleResult(BOOL success) 
{
	if (success)
		printf("Success!\n");
	else
		printf("Error: %d\n", ::GetLastError());
}

int wmain(int argc, const wchar_t* argv[]) 
{
	if (argc < 3) {
		printf("Usage: DelProtectClient.exe <method> <filename>\n");
		printf("\tMethod: 1=DeleteFile, 2=delete on close, 3=SetFileInformation.\n");
		return 0;
	}

	auto method = _wtoi(argv[1]);
	auto filename = argv[2];
	HANDLE hFile;
	BOOL success;

	switch (method) {
	case 1:
		printf("Using DeleteFile:\n");
		success = ::DeleteFile(filename);
		HandleResult(success);
		break;

	case 2:
		printf("Using CreateFile with FILE_FLAG_DELETE_ON_CLOSE:\n");
		hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		HandleResult(hFile != INVALID_HANDLE_VALUE);
		::CloseHandle(hFile);
		break;

	case 3:
		printf("Using SetFileInformationByHandle:\n");
		FILE_DISPOSITION_INFO info;
		info.DeleteFile = TRUE;
		hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		success = ::SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(info));
		HandleResult(success);
		::CloseHandle(hFile);
		break;
	}

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
