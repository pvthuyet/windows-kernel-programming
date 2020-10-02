// FileBackupRestore.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>

int Error(const char* text) 
{
	printf("%s (%d)\n", text, ::GetLastError());
	return 1;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2) 
	{
		printf("Usage: FileRestore <filename>\n");
		return 0;
	}

	// locate the backup stream
	std::wstring stream(argv[1]);
	stream += L":backup";
	HANDLE hSource = INVALID_HANDLE_VALUE;
	HANDLE hTarget = INVALID_HANDLE_VALUE;
	void* buffer = nullptr;

	do
	{
		hSource = ::CreateFile(stream.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hSource == INVALID_HANDLE_VALUE) {
			Error("Failed to locate backup");
			break;
		}

		hTarget = ::CreateFile(argv[1], GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hTarget == INVALID_HANDLE_VALUE) {
			Error("Failed to locate file");
			break;
		}

		LARGE_INTEGER size{ 0 };
		if (!::GetFileSizeEx(hSource, &size)) {
			Error("Failed to get file size");
			break;
		}

		ULONG bufferSize = (ULONG)min((LONGLONG)1 << 21, size.QuadPart);
		buffer = VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!buffer) {
			Error("Failed to allocate buffer");
			break;
		}

		DWORD bytes{0};
		while (size.QuadPart > 0)
		{
			if (!::ReadFile(hSource, buffer, (DWORD)(min((LONGLONG)bufferSize, size.QuadPart)), &bytes, nullptr)) 
			{
				Error("Failed to read data");
				break;
			}

			if (!::WriteFile(hTarget, buffer, bytes, &bytes, nullptr))
			{
				Error("Failed to write data");
				break;
			}
			size.QuadPart -= bytes;
		}

	} while (false);

	printf("Restore successful!\n");

	if (hSource != INVALID_HANDLE_VALUE) {
		::CloseHandle(hSource);
	}

	if (hTarget != INVALID_HANDLE_VALUE) {
		::CloseHandle(hTarget);
	}

	if (buffer) {
		::VirtualFree(buffer, 0, MEM_DECOMMIT | MEM_RELEASE);
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
