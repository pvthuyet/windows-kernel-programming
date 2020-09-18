// SysMonClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include "..\SysMon\SysMonCommon.h"
#include <tchar.h>

int Error(const char*);
void DisplayTime(const LARGE_INTEGER& time);
void DisplayInfo(BYTE* buffer, DWORD size);

int main()
{
    auto hFile = ::CreateFile(_T("\\\\.\\SysMon"), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return Error("Failed to open file");
    }

    BYTE buffer[1 << 16] = { 0 }; //64k buffer
    while (true)
    {
        DWORD bytes;
        if (!::ReadFile(hFile, buffer, sizeof(buffer), &bytes, nullptr))
        {
            return Error("Failed to read");
        }
        if (0 != bytes)
        {
            DisplayInfo(buffer, bytes);
        }
        ::Sleep(200);
    }
    ::CloseHandle(hFile);
    return 0;
}

int Error(const char* msg)
{
    printf_s("%s: error=%d\n", msg, ::GetLastError());
    return 1;
}

void DisplayInfo(BYTE* buffer, DWORD size)
{
    auto count = size;
    while (count > 0)
    {
        auto header = (ItemHeader*)buffer;
        switch (header->Type)
        {
        case ItemType::ProcessCreate:
            {
                DisplayTime(header->Time);
                auto info = (ProcessCreateInfo*)buffer;
                std::wstring commandLine((WCHAR*)(buffer + info->CommandLineOffset), info->CommandLineLength);
                printf_s("Process %d created. Command line: %ws\n", info->ProcessId, commandLine.c_str());
            }
            break;

        case ItemType::ProcessExit:
            {
                DisplayTime(header->Time);
                auto info = (ProcessExitInfo*)buffer;
                printf_s("Process %d exited\n", info->ProcessId);
            }
            break;

        case ItemType::ThreadCreate:
            {
                //DisplayTime(header->Time);
                //auto info = (ThreadCreateExitInfo*)buffer;
                //printf_s("Thread %d created in process %d\n", info->ThreadId, info->ProcessId);
            }
            break;

        case ItemType::ThreadExit:
            {
                //DisplayTime(header->Time);
                //auto info = (ThreadCreateExitInfo*)buffer;
                //printf_s("Thread %d exited from process %d\n", info->ThreadId, info->ProcessId);
            }
            break;

        case ItemType::ImageLoad:
            {
                DisplayTime(header->Time);
                auto info = (ImageLoadInfo*)buffer;
                printf_s("Image loaded into process %d at address 0x%p (%ws)\n", info->ProcessId, info->LoadAddress, info->ImageFileName);
            }
            break;

        default:
            break;
        }
        buffer += header->Size;
        count -= header->Size;
    }
}

void DisplayTime(const LARGE_INTEGER& time)
{
    SYSTEMTIME st;
    ::FileTimeToSystemTime((FILETIME*)&time, &st);
    printf_s("%02d:%02d:%02d:%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
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
