#include "pch.h"
#include "FastMutex.h"

void FastMutex::init()
{
	ExInitializeFastMutex(&_mutex);
}

void FastMutex::lock()
{
	ExAcquireFastMutex(&_mutex);
}

void FastMutex::unlock()
{
	ExReleaseFastMutex(&_mutex);
}