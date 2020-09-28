#pragma once

bool IsBackupDirectory(_In_ PCUNICODE_STRING directory);
NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects);

