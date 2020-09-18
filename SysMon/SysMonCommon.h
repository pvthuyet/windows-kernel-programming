#pragma once

enum class ItemType : short
{
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad
};

struct ItemHeader
{
	ItemType Type;
	USHORT Size;
	LARGE_INTEGER Time;
};