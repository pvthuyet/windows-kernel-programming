#include <ntddk.h>
#include "KDevMon.h"
#include "DevMonManager.h"

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
	return STATUS_SUCCESS;
}