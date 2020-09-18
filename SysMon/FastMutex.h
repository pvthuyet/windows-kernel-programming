#pragma once
class FastMutex
{
public:
	void init();
	void lock();
	void unlock();

private:
	FAST_MUTEX _mutex;
};

