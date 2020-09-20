#include "pch.h"
#include "FastMutex.h"
#include "ProcessProtect.h"

Globals g_Data;

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegisteryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegisteryPath);
	return STATUS_SUCCESS;
}