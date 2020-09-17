#include "pch.h"

#define DRIVER_PREFIX	"Zero: "

// Prototypes
void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject);
DRIVER_DISPATCH ZeroCreateClose, ZeroRead, ZeroWrite;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	KdPrint((DRIVER_PREFIX "DriverEntry ENTER\n"));
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = ZeroUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	PDEVICE_OBJECT DeviceObject = nullptr;
	auto status = STATUS_SUCCESS;

	do
	{
		status = IoCreateDevice(DriverObject, 
			0, 
			&devName, 
			FILE_DEVICE_UNKNOWN,
			0,
			FALSE,
			&DeviceObject);

		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed to create device (0x%08X)\n", status));
			break;
		}

		// set up Direct I/O
		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed to create symbolic link (0x%08X)\n", status));
			break;
		}
	} while(false);

	if (!NT_SUCCESS(status))
	{
		if (DeviceObject)
		{
			IoDeleteDevice(DeviceObject);
		}
	}

	KdPrint((DRIVER_PREFIX "DriverEntry EXIT\n"));
	return status;
}

void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, 0);
	return status;
}

_Use_decl_annotations_
NTSTATUS ZeroCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS ZeroRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	KdPrint((DRIVER_PREFIX "ZeroRead ENTER\n"));
	UNREFERENCED_PARAMETER(DeviceObject);
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	if (0 == len)
	{
		return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);
	}

	auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer)
	{
		return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);
	}

	RtlZeroMemory(buffer, len);
	KdPrint((DRIVER_PREFIX "ZeroRead EXIT\n"));
	return CompleteIrp(Irp, STATUS_SUCCESS);
}

_Use_decl_annotations_
NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Write.Length;
	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}