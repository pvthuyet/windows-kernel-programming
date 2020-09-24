#include "pch.h"
#include "FastMutex.h"
#include "ProcessProtect.h"
#include "ProcessProtectCommon.h"
#include "AutoLock.h"
#include "ke_logger.h"

DRIVER_UNLOAD ProcessProtectUnload;
DRIVER_DISPATCH ProcessProtectCreateClose, ProcessProtectDeviceControl;
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(_In_ PVOID, _Inout_ POB_PRE_OPERATION_INFORMATION);
bool FindProcess(ULONG pid);
bool AddProcess(ULONG pid);
bool RemoveProcess(ULONG pid);

Globals g_Data;

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegisteryPath)
{
	LOGENTER;
	g_Data.Init();
	UNREFERENCED_PARAMETER(RegisteryPath);

	OB_OPERATION_REGISTRATION operations[] = {
		{
			PsProcessType,
			OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
			OnPreOpenProcess,
			nullptr
		}
	};

	OB_CALLBACK_REGISTRATION reg = {
		OB_FLT_REGISTRATION_VERSION,
		1,
		RTL_CONSTANT_STRING(L"12345.6171"),
		nullptr,
		operations
	};

	auto status = STATUS_SUCCESS;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\" PROCESS_PROTECT_NAME);
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" PROCESS_PROTECT_NAME);
	PDEVICE_OBJECT DeviceObject = nullptr;

	do
	{
		status = ObRegisterCallbacks(&reg, &g_Data.RegHandle);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to register callbacks (status=%08X)\n", status));
			break;
		}

		status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to create device object (status=%08X)\n", status));
			break;
		}

		status = IoCreateSymbolicLink(&symName, &deviceName);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to create symbolic link (status=%08X)\n", status));
			break;
		}

	} while(false);

	if (!NT_SUCCESS(status))
	{
		if (DeviceObject)
		{
			IoDeleteDevice(DeviceObject);
		}

		if (g_Data.RegHandle)
		{
			ObUnRegisterCallbacks(g_Data.RegHandle);
		}
	}

	DriverObject->DriverUnload = ProcessProtectUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProcessProtectCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcessProtectDeviceControl;

	LOGEXIT;
	return status;
}

VOID ProcessProtectUnload(_In_ DRIVER_OBJECT* DriverObject)
{
	ObUnRegisterCallbacks(g_Data.RegHandle);
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" PROCESS_PROTECT_NAME);
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ProcessProtectCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ProcessProtectDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	auto len = 0;

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PROCESS_PROTECT_BY_PID:
		{
			auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (size % sizeof(ULONG) != 0)
			{
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
			AutoLock locker(g_Data.Lock);
			for (int i = 0; i < size / sizeof(ULONG); ++i)
			{
				auto pid = data[i];
				if (0 == pid)
				{
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				if (FindProcess(pid))
				{
					continue;
				}

				if (MaxPids == g_Data.PidsCount)
				{
					status = STATUS_TOO_MANY_CONTEXT_IDS;
					break;
				}

				if (!AddProcess(pid))
				{
					status = STATUS_UNSUCCESSFUL;
					break;
				}

				len += sizeof(ULONG);
			}
		}
		break;

	case IOCTL_PROCESS_UNPROTECT_BY_PID:
		{
			auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
			if (size % sizeof(ULONG) != 0)
			{
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
			AutoLock locker(g_Data.Lock);
			for (int i = 0; i < size / sizeof(ULONG); ++i)
			{
				auto pid = data[i];
				if (0 == pid)
				{
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				if (!RemoveProcess(pid))
				{
					continue;
				}

				len += sizeof(ULONG);

				if (0 == g_Data.PidsCount)
				{
					break;
				}
			}
		}
		break;

	case IOCTL_PROCESS_PROTECT_CLEAR:
		{
			AutoLock locker(g_Data.Lock);
			RtlZeroMemory(g_Data.Pids, sizeof(g_Data.Pids));
			g_Data.PidsCount = 0;
		}
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	LOGEXIT;
	return status;
}

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(_In_ PVOID RegistrationContext, _Inout_ POB_PRE_OPERATION_INFORMATION info)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(RegistrationContext);
	if (info->KernelHandle)
	{
		return OB_PREOP_SUCCESS;
	}
	auto process = (PEPROCESS)info->Object;
	auto pid = HandleToUlong(PsGetProcessId(process));

	AutoLock locker(g_Data.Lock);
	if (FindProcess(pid))
	{
		info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
	}

	LOGEXIT;
	return OB_PREOP_SUCCESS;
}

bool FindProcess(ULONG pid)
{
	for (int i = 0; i < MaxPids; ++i)
	{
		if (pid == g_Data.Pids[i])
		{
			return true;
		}
	}
	return false;
}

bool AddProcess(ULONG pid)
{
	for (int i = 0; i < MaxPids; ++i)
	{
		if (0 == g_Data.Pids[i]) // empty slot
		{
			g_Data.Pids[i] = pid;
			g_Data.PidsCount++;
			return true;
		}
	}
	return false; // all slots are busy
}

bool RemoveProcess(ULONG pid)
{
	for (int i = 0; i < MaxPids; ++i)
	{
		if (pid == g_Data.Pids[i])
		{
			g_Data.Pids[i] = 0;
			g_Data.PidsCount--;
			return true;
		}
	}
	return false; // not found
}
