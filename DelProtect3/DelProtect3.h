#pragma once

#define DRIVER_PREFIX "[DelProtect3] "
#define DRIVER_TAG 'DelP'

struct DirectoryEntry 
{
	UNICODE_STRING DosName;
	UNICODE_STRING NtName;

	void Free() 
	{
		if (DosName.Buffer) {
			ExFreePoolWithTag(DosName.Buffer, DRIVER_TAG);
			DosName.Buffer = nullptr;
		}

		if (NtName.Buffer) {
			ExFreePoolWithTag(NtName.Buffer, DRIVER_TAG);
			NtName.Buffer = nullptr;
		}
	}
};