#include "pch.h"
#include "ZeroCommon.h"

// global variable
long long g_TotalRead{0}, g_TotalWritten{0};

// Prototypes
void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject);
DRIVER_DISPATCH ZeroCreateClose, ZeroRead, ZeroWrite, ZeroDeviceControl;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = ZeroUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	PDEVICE_OBJECT DeviceObject = nullptr;
	auto status = STATUS_SUCCESS;
	auto symLinkCreated = false;

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
		symLinkCreated = true;

	} while(false);

	if (!NT_SUCCESS(status))
	{
		if (symLinkCreated)
		{
			IoDeleteSymbolicLink(&symLink);
		}

		if (DeviceObject)
		{
			IoDeleteDevice(DeviceObject);
		}
	}

	LOGEXIT;
	return status;
}

void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	LOGENTER;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
	LOGEXIT;
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

_Use_decl_annotations_
NTSTATUS ZeroCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);
	LOGEXIT;
	return CompleteIrp(Irp);
}

_Use_decl_annotations_
NTSTATUS ZeroRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	KE_DBG_PRINT(KEDBG_TRACE_DEBUG, (DRIVER_PREFIX "Read file length: %lu\n", len));
	if (0 == len)
	{
		KE_DBG_PRINT(KEDBG_TRACE_ERROR, (DRIVER_PREFIX "Invalid buffer size\n"));
		return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);
	}

	auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer)
	{
		KE_DBG_PRINT(KEDBG_TRACE_ERROR, (DRIVER_PREFIX "Insufficient resources\n"));
		return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);
	}

	RtlZeroMemory(buffer, len);
	InterlockedAdd64(&g_TotalRead, len);
	KE_DBG_PRINT(KEDBG_TRACE_DEBUG, (DRIVER_PREFIX "Read: %lu, Total read: %lld\n", len, g_TotalRead));

	LOGEXIT;
	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}

_Use_decl_annotations_
NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Write.Length;
	InterlockedAdd64(&g_TotalWritten, len);
	KE_DBG_PRINT(KEDBG_TRACE_DEBUG, (DRIVER_PREFIX "Write: %lu, Total write: %lld\n", len, g_TotalWritten));

	LOGEXIT;
	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}

_Use_decl_annotations_
NTSTATUS ZeroDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto& dic = stack->Parameters.DeviceIoControl;
	if (IOCTL_ZERO_GET_STATS != dic.IoControlCode)
	{
		KE_DBG_PRINT(KEDBG_TRACE_ERROR, (DRIVER_PREFIX "Invalid device request\n"));
		return CompleteIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}

	if (dic.OutputBufferLength < sizeof(ZeroStats))
	{
		KE_DBG_PRINT(KEDBG_TRACE_ERROR, (DRIVER_PREFIX "Buffer too small\n"));
		return CompleteIrp(Irp, STATUS_BUFFER_TOO_SMALL);
	}

	auto stats = (ZeroStats*)Irp->AssociatedIrp.SystemBuffer;
	stats->TotalRead = g_TotalRead;
	stats->TotalWritten = g_TotalWritten;
	LOGEXIT;

	return CompleteIrp(Irp, STATUS_SUCCESS, sizeof(ZeroStats));
}