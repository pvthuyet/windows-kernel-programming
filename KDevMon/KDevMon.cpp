#include <ntddk.h>
#include "KDevMon.h"
#include "DevMonManager.h"

DRIVER_UNLOAD DevMonUnload;
DRIVER_DISPATCH DevMonDeviceControl, HandleFilterFunction;

DevMonManager g_Data;

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR information = 0);

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

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR information) 
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = information;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS HandleFilterFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	if (DeviceObject == g_Data.CDO)
	{
		switch (IoGetCurrentIrpStackLocation(Irp)->MajorFunction)
		{
		case IRP_MJ_CREATE:
		case IRP_MJ_CLOSE:
				return CompleteRequest(Irp);
			
		case IRP_MJ_DEVICE_CONTROL:
				return DevMonDeviceControl(DeviceObject, Irp);
		}
		return CompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}

	auto ext = (DeviceExtension*)DeviceObject->DeviceExtension;
	
	auto thread = Irp->Tail.Overlay.Thread;
	HANDLE tid = nullptr, pid = nullptr;
	if (thread)
	{
		tid = PsGetThreadId(thread);
		pid = PsGetThreadProcessId(thread);
	}

	auto stack = IoGetCurrentIrpStackLocation(Irp);
}