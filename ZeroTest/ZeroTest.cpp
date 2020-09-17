// TestZero.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "..\include\scope.hpp"
#include "..\include\handle_deleter.hpp"
#include <iostream>
#include <Windows.h>

using std::experimental::make_unique_resource_checked;
int Error(const char*);

int main()
{
	auto unr = make_unique_resource_checked(CreateFile(L"\\\\.\\Zero",
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr),
		INVALID_HANDLE_VALUE,
		fibo::CloseHandleDeleter{}
	);

	if (!unr.valid())
	{
		return Error("Failed to open device");
	}

	// Test read
	std::cout << "Test read\n";
	BYTE buffer[64] = { 0 };
	for (int i = 0; i < sizeof(buffer); ++i)
	{
		buffer[i] = i + 1;
	}

	DWORD bytes;
	auto ok = ::ReadFile(unr.get(), buffer, sizeof(buffer), &bytes, nullptr);
	if (!ok)
	{
		return Error("Failed to read");
	}

	if (sizeof(buffer) != bytes)
	{
		printf_s("Wrong number of bytes (buffer size: %zu, read bytes: %lu)\n", sizeof(buffer), bytes);
	}

	// check if buffer data sum is zero
	long total = 0;
	for (auto n : buffer)
	{
		total += n;
	}
	if (0 != total)
	{
		std::cout << "Wrong data\n";
	}
	else
	{
		printf_s("Read number of bytes %lu\n", bytes);
	}

	// Test write
	std::cout << "Test write\n";
	BYTE buffer2[1024] = { 0 };
	ok = ::WriteFile(unr.get(), buffer2, sizeof(buffer2), &bytes, nullptr);
	if (!ok)
	{
		return Error("Failed to write");
	}

	if (sizeof(buffer2) != bytes)
	{
		std::cout << "Wrong byte count\n";
	}
	else
	{
		printf_s("Write number of bytes %lu\n", bytes);
	}
	return 0;
}

int Error(const char* msg)
{
    printf_s("%s: error=%d\n", msg, ::GetLastError());
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
