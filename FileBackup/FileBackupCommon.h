#pragma once

#define FILE_BACKUP_PORT L"\\FileBackupPort"

struct FileBackupPortMessage 
{
	USHORT FileNameLength;
	WCHAR FileName[1];
};