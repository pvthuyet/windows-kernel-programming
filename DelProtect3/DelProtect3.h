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
			ExFreePool(DosName.Buffer);
			DosName.Buffer = nullptr;
		}

		if (NtName.Buffer) {
			ExFreePool(NtName.Buffer);
			NtName.Buffer = nullptr;
		}
	}
};