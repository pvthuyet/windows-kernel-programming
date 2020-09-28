#pragma once

struct FileContext {
	fibo::kernel::Mutex Lock;
	UNICODE_STRING FileName;
	BOOLEAN Written;
};

void FileContextCleanup(_In_ PFLT_CONTEXT Context, _In_ FLT_CONTEXT_TYPE /* ContextType */);
