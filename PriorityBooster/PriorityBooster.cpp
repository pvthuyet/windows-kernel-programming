#include <ntifs.h>
#include <ntddk.h>
#include "PriorityBoosterCommon.h"

// Prototypes
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegisteryPath)
{
	UNREFERENCED_PARAMETER(RegisteryPath);
	KdPrint(("PriorityBooster DriverEntry Started\n"));

	DriverObject->DriverUnload = PriorityBoosterUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	// Create device object
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create device (0x%08X)\n", status));
		return status;
	}

	// Create symbolic link
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbolic link (0x%08X)\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	KdPrint(("PriorityBooster DriverEntry completed successfully\n"));

	return STATUS_SUCCESS;
}

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
		{
			auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (len < sizeof(ThreadData))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			if (nullptr == data)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			__try
			{
				if (data->Priority < 1 || data->Priority > 31)
				{
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				PETHREAD Thread = nullptr;
				status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
				if (!NT_SUCCESS(status))
				{
					break;
				}

				KeSetPriorityThread(Thread, data->Priority);
				ObDereferenceObject(Thread);
				KdPrint(("Thread Priority change for %d to %d succeeded!\n", data->ThreadId, data->Priority));
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				// something wrong with the buffer
				status = STATUS_ACCESS_VIOLATION;
			}
		}
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}