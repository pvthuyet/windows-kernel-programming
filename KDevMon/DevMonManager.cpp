#include <ntddk.h>
#include "DevMonManager.h"
#include "KDevMon.h"

void DevMonManager::Init(PDRIVER_OBJECT driver) 
{
	Lock.init();
	DriverObject = driver;
}

NTSTATUS DevMonManager::AddDevice(PCWSTR name)
{
	fibo::kernel::LockGuard locker(Lock);
	if (MonitoredDeviceCount == MaxMonitoredDevices) {
		return STATUS_TOO_MANY_NAMES;
	}

	if (FindDevice(name) >= 0) {
		return STATUS_SUCCESS;
	}

	for (int i = 0; i < MaxMonitoredDevices; i++)
	{
		if (Devices[i].DeviceObject == nullptr)
		{
			UNICODE_STRING targetName;
			RtlInitUnicodeString(&targetName, name);

			PFILE_OBJECT FileObject;
			PDEVICE_OBJECT LowerDeviceObject = nullptr;
			auto status = IoGetDeviceObjectPointer(&targetName, FILE_READ_DATA, &FileObject, &LowerDeviceObject);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Failed to get device object pointer (%ws) (0x%8X)\n", name, status));
				return status;
			}

			PDEVICE_OBJECT DeviceObject = nullptr;
			WCHAR* buffer = nullptr;

			do
			{
				status = IoCreateDevice(DriverObject, sizeof(DeviceExtension), nullptr,
					FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
				if (!NT_SUCCESS(status)) {
					break;
				}

				// allocate buffer to copy device name
				buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetName.Length, DRIVER_TAG);
				if (!buffer) {
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				auto ext = (DeviceExtension*)DeviceObject->DeviceExtension;
				DeviceObject->Flags |= LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);

				DeviceObject->DeviceType = LowerDeviceObject->DeviceType;
				Devices[i].DeviceName.Buffer = buffer;
				Devices[i].DeviceName.MaximumLength = targetName.Length;
				RtlCopyUnicodeString(&Devices[i].DeviceName, &targetName);
				Devices[i].DeviceObject = DeviceObject;
				//++ todo



			} while(false);
		}
	}


	return STATUS_SUCCESS;
}

int DevMonManager::FindDevice(PCWSTR name) const
{
	UNICODE_STRING uname;
	RtlInitUnicodeString(&uname, name);
	for (int i = 0; i < MaxMonitoredDevices; i++) 
	{
		auto& device = Devices[i];
		if (device.DeviceObject && RtlEqualUnicodeString(&device.DeviceName, &uname, TRUE)) {
			return i;
		}
	}
	return -1;
}

bool DevMonManager::RemoveDevice(PCWSTR name)
{
	fibo::kernel::LockGuard locker(Lock);
	int index = FindDevice(name);
	if (index < 0) {
		return false;
	}

	return RemoveDevice(index);
}

void DevMonManager::RemoveAllDevices()
{
	fibo::kernel::LockGuard locker(Lock);
	for (int i = 0; i < MaxMonitoredDevices; ++i) {
		RemoveDevice(i);
	}
}

bool DevMonManager::RemoveDevice(int index)
{
	auto& device = Devices[index];
	if (nullptr == device.DeviceObject) {
		return false;
	}

	ExFreePool(device.DeviceName.Buffer);
	IoDetachDevice(device.LowerDeviceObject);
	IoDeleteDevice(device.DeviceObject);
	device.DeviceObject = nullptr;
	MonitoredDeviceCount--;

	return true;
}