#include <ntddk.h>
#include "KDevMon.h"
#include "DevMonManager.h"
#include "KDevMonCommon.h"

DRIVER_UNLOAD DevMonUnload;
DRIVER_DISPATCH DevMonDeviceControl, HandleFilterFunction;
PCSTR MajorFunctionToString(UCHAR major);

DevMonManager g_Data;

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR information = 0);

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING devName = RTL_CONSTANT_STRING(DEVICE_NAME);
	PDEVICE_OBJECT DeviceObject;

	auto status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	UNICODE_STRING linkName = RTL_CONSTANT_STRING(DEVICE_LINK);
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
	UNICODE_STRING linkName = RTL_CONSTANT_STRING(DEVICE_LINK);
	IoDeleteSymbolicLink(&linkName);
	NT_ASSERT(g_Data.CDO);
	IoDeleteDevice(g_Data.CDO);

	g_Data.RemoveAllDevices();
}

PCSTR MajorFunctionToString(UCHAR major) {
	static const char* strings[] = {
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
	};
	NT_ASSERT(major <= IRP_MJ_MAXIMUM_FUNCTION);
	return strings[major];
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

	DbgPrint(("driver: %wZ: PID: %d, TID: %d, MJ=%d (%s)\n",
		&ext->LowerDeviceObject->DriverObject->DriverName,
		HandleToUlong(pid), HandleToUlong(tid),
		stack->MajorFunction, MajorFunctionToString(stack->MajorFunction)));

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(ext->LowerDeviceObject, Irp);
}

NTSTATUS DevMonDeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	auto code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code)
	{
	case IOCTL_DEVMON_ADD_DEVICE:
	case IOCTL_DEVMON_REMOVE_DEVICE:
		{
			auto buffer = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
			auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (buffer == nullptr || len > 512)
			{
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			buffer[len / sizeof(WCHAR) - 1] = L'\0';
			if (code == IOCTL_DEVMON_ADD_DEVICE) {
				status = g_Data.AddDevice(buffer);
			}
			else {
				auto removed = g_Data.RemoveDevice(buffer);
				status = removed ? STATUS_SUCCESS : STATUS_NOT_FOUND;
			}
			break;
		}

	case IOCTL_DEVMON_REMOVE_ALL:
		{
			g_Data.RemoveAllDevices();
			status = STATUS_SUCCESS;
			break;
		}

	default:
		break;
	}

	return CompleteRequest(Irp, status);
}