#include "pch.h"
#include "SysMon.h"
#include "SysMonCommon.h"
#include "AutoLock.h"
#include "..\include\ke_logger.h"

DRIVER_UNLOAD SysMonUnload;
DRIVER_DISPATCH SysMonCreateClose, SysMonRead;
VOID OnProcessNotify(_Inout_ PEPROCESS Process, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo);
VOID OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create);
VOID OnImageLoadNotify(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo);
VOID PushItem(LIST_ENTRY* entry);
EX_CALLBACK_FUNCTION OnRegistryNotify;

Globals g_Globals;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(RegistryPath);

	auto status = STATUS_SUCCESS;

	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.Mutex.init();

	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	bool symLinkCreated = false;
	bool processCallbacks = false, threadCallbacks = false, loadImageCallback = false;

	do
	{
		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\sysmon");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to create sym link (0x%08X)\n", status));
			break;
		}
		symLinkCreated = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to register process callback (0x%08X)\n", status));
			break;
		}
		processCallbacks = true;

		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to set thread callback (status=%08X)\n", status));
			break;
		}
		threadCallbacks = true;

		status = PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to set image load callback (status=%08X)\n", status));
			break;
		}
		loadImageCallback = true;

		UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.124");
		status = CmRegisterCallbackEx(OnRegistryNotify, &altitude, DriverObject, nullptr, &g_Globals.RegCookie, nullptr);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "failed to set registry callback (status=%08X)\n", status));
			break;
		}

	} while(false);

	if (!NT_SUCCESS(status))
	{
		if (loadImageCallback)
		{
			PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
		}
		if (threadCallbacks)
		{
			PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
		}
		if (processCallbacks)
		{
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		}
		if (symLinkCreated)
		{
			IoDeleteSymbolicLink(&symLink);
		}
		if (DeviceObject)
		{
			IoDeleteDevice(DeviceObject);
		}
	}

	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;

	LOGEXIT;
	return status;
}

VOID SysMonUnload(PDRIVER_OBJECT DriverObject)
{
	LOGENTER;
	CmUnRegisterCallback(g_Globals.RegCookie);
	PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_Globals.ItemsHead))
	{
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}

	LOGEXIT;
}

NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);

	LOGEXIT;
	return STATUS_SUCCESS;
}

NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(DeviceObject);
	
	auto status = STATUS_SUCCESS;
	auto count = 0;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	NT_ASSERT(Irp->MdlAddress);

	auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		KdPrint((DRIVER_PREFIX "Failed MmGetSystemAddressForMdl: insufficient resources\n"));
	}
	else
	{
		AutoLock<FastMutex> lock(g_Globals.Mutex);
		while (true)
		{
			if (IsListEmpty(&g_Globals.ItemsHead))
			{
				break;
			}

			auto entry = RemoveHeadList(&g_Globals.ItemsHead);
			auto info = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
			auto size = info->Data.Size;
			if (len < size)
			{
				// user's buffer full, insert item back
				InsertHeadList(&g_Globals.ItemsHead, entry);
				KdPrint((DRIVER_PREFIX "Length buffer size is less than Item data size\n"));
				break;
			}

			g_Globals.ItemCount--;
			memcpy_s(buffer, len, &info->Data, size);
			len -= size;
			buffer += size;
			count += size;
			ExFreePool(info);
			KdPrint((DRIVER_PREFIX "Success read data length: %d\n", size));
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = count;
	IoCompleteRequest(Irp, 0);

	LOGEXIT;
	return status;
}

VOID OnProcessNotify(_Inout_ PEPROCESS Process, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(Process);
	if (CreateInfo)
	{
		// Process created
		USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
		USHORT commandLineSize = 0;
		if (CreateInfo->CommandLine)
		{
			commandLineSize = CreateInfo->CommandLine->Length;
			allocSize += commandLineSize;
		}
		auto info = (FullItem<ProcessCreateInfo>*)ExAllocatePoolWithTag(PagedPool, allocSize, DRIVER_TAG);
		if (nullptr == info)
		{
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			return;
		}
		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessCreate;
		item.Size = sizeof(ProcessCreateInfo) + commandLineSize;
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		if (commandLineSize > 0)
		{
			memcpy_s((UCHAR*)&item + sizeof(item), commandLineSize, CreateInfo->CommandLine->Buffer, commandLineSize);
			item.CommandLineLength = commandLineSize / sizeof(WCHAR);
			item.CommandLineOffset = sizeof(item);
		}
		else
		{
			item.CommandLineLength = 0;
		}
		PushItem(&info->Entry);
	}
	else
	{
		// Process exited
		auto info = (FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (nullptr == info)
		{
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			return;
		}
		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.Size = sizeof(ProcessExitInfo);
		item.ProcessId = HandleToULong(ProcessId);
		PushItem(&info->Entry);
	}
	LOGEXIT;
}

VOID OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create)
{
	LOGENTER;
	auto size = sizeof(FullItem<ThreadCreateExitInfo>);
	auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
	if (nullptr == info)
	{
		KdPrint((DRIVER_PREFIX "Failed to allocate memory\n"));
		return;
	}
	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.Time);
	item.Size = sizeof(item);
	item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;
	item.ProcessId = HandleToULong(ProcessId);
	item.ThreadId = HandleToUlong(ThreadId);

	PushItem(&info->Entry);
	LOGEXIT;
}

VOID OnImageLoadNotify(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo)
{
	LOGENTER;
	if (nullptr == ProcessId)
	{
		return;
	}
	auto size = sizeof(FullItem<ImageLoadInfo>);
	auto info = (FullItem<ImageLoadInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
	if (nullptr == info)
	{
		KdPrint((DRIVER_PREFIX "Failed to allocate memory\n"));
		return;
	}
	RtlZeroMemory(info, size);

	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.Time);
	item.Size = sizeof(item);
	item.Type = ItemType::ImageLoad;
	item.ProcessId = HandleToULong(ProcessId);
	item.ImageSize = ImageInfo->ImageSize;
	item.LoadAddress = ImageInfo->ImageBase;

	if (FullImageName)
	{
		memcpy_s(item.ImageFileName, MaxImageFileSize * sizeof(WCHAR), FullImageName->Buffer, FullImageName->Length);
	}
	else
	{
		wcscpy_s(item.ImageFileName, L"(unknown)");
	}
	PushItem(&info->Entry);
	LOGEXIT;
}

NTSTATUS OnRegistryNotify(PVOID context, PVOID arg1, PVOID arg2)
{
	LOGENTER;
	UNREFERENCED_PARAMETER(context);
	static constexpr WCHAR machine[] = L"\\REGISTRY\\MACHINE\\";
	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1)
	{
	case RegNtPostSetValueKey:
		{
			auto args = static_cast<REG_POST_OPERATION_INFORMATION*>(arg2);
			if (!NT_SUCCESS(args->Status))
			{
				break;
			}
			PCUNICODE_STRING name;
			auto status = CmCallbackGetKeyObjectIDEx(&g_Globals.RegCookie, args->Object, nullptr, &name, 0);
			if (!NT_SUCCESS(status))
			{
				break;
			}
			auto cmp = wcsncmp(name->Buffer, machine, ARRAYSIZE(machine)-1);
			if (0 == cmp)
			{
				auto preInfo = (REG_SET_VALUE_KEY_INFORMATION*)args->PreInformation;
				NT_ASSERT(preInfo);
				auto size = sizeof(FullItem<RegistrySetValueInfo>);
				auto info = (FullItem<RegistrySetValueInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
				if (nullptr == info)
				{
					break;
				}
				RtlZeroMemory(info, size);
				auto& item = info->Data;
				KeQuerySystemTimePrecise(&item.Time);
				item.Size = sizeof(item);
				item.Type = ItemType::RegistrySetValue;
				wcsncpy_s(item.KeyName, name->Buffer, name->Length/sizeof(WCHAR));
				wcsncpy_s(item.ValueName, preInfo->ValueName->Buffer, preInfo->ValueName->Length / sizeof(WCHAR));
				item.DataType = preInfo->Type;
				item.DataSize = preInfo->DataSize;
				item.ProcessId = HandleToULong(PsGetCurrentProcessId());
				item.ThreadId = HandleToULong(PsGetCurrentThreadId());
				memcpy_s(item.Data, sizeof(item.Data), preInfo->Data, item.DataSize);
				PushItem(&info->Entry);
			}
			CmCallbackReleaseKeyObjectIDEx(name);
		}
		break;

	default:
		break;
	}
	
	LOGEXIT;
	return STATUS_SUCCESS;
}

VOID PushItem(LIST_ENTRY* entry)
{
	AutoLock<FastMutex> lock(g_Globals.Mutex);
	if (g_Globals.ItemCount > 1024)
	{
		// too many items, remove oldest one
		auto head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;
		auto item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
		ExFreePool(item);
	}
	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}