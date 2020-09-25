#pragma once

#define IOCTL_DELPROTECT_ADD_EXE		CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_REMOVE_EXE		CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_CLEAR			CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)

constexpr wchar_t DEVICE_NAME[]			= L"\\device\\delprotect2";
constexpr wchar_t SYM_LINK_NAME[]		= L"\\??\\delprotect2";
constexpr wchar_t USER_FILE_NAME[]		= L"\\\\.\\delprotect2";