#include <ntddk.h>
#include "KDevMon.h"
#include "DevMonManager.h"

DRIVER_UNLOAD DevMonUnload;
DRIVER_DISPATCH DevMonDeviceControl, HandleFilterFunction;

DevMonManager g_Data;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\KDevMon");
	PDEVICE_OBJECT DeviceObject;

	auto status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\KDevMon");
	status = IoCreateSymbolicLink(&linkName, &devName);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject);
		return status;
	}

	DriverObject->DriverUnload = DevMonUnload;
	for (auto& func : DriverObject->MajorFunction)
	{
		func = HandleFilterFunction;
	}

	g_Data.CDO = DeviceObject;
	g_Data.Init(DriverObject);

	return status;
}

void DevMonUnload(PDRIVER_OBJECT DriverObject) 
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\KDevMon");
	IoDeleteSymbolicLink(&linkName);
	NT_ASSERT(g_Data.CDO);
	IoDeleteDevice(g_Data.CDO);

	g_Data.RemoveAllDevices();
}