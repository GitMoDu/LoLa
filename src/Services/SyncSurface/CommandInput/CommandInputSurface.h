// CommandInputSurface.h

#ifndef _COMMANDINPUTSURFACE_h
#define _COMMANDINPUTSURFACE_h

#include <BitTracker.h>
#include <Services\SyncSurface\ITrackedSurface.h>


#define COMMAND_INPUT_BLOCK_COUNT 2
#define COMMAND_INPUT_DATA_SIZE (COMMAND_INPUT_BLOCK_COUNT*SYNC_SURFACE_BLOCK_SIZE)

#define PACKET_DEFINITION_SYNC_COMMAND_INPUT_BASE_HEADER (PACKET_DEFINITION_PING_HEADER+1)

#define SYNC_COMMAND_INPUT_DIRECTION_AND_PROPULSION_BLOCK_INDEX 0
#define SYNC_COMMAND_INPUT_TRIM_BLOCK_INDEX 1

#define SYNC_COMMAND_INPUT_DIRECTION_DATA_OFFSET 0
#define SYNC_COMMAND_INPUT_PROPULSION_DATA_OFFSET 1

class CommandInputSurface : public ITrackedSurfaceNotify
{
private:
	BitTracker8 Tracker = BitTracker8(COMMAND_INPUT_BLOCK_COUNT);
	uint8_t Data[COMMAND_INPUT_DATA_SIZE];

	uint16_t Grunt16;

	uint16_t LastDirection = 0;
	uint16_t LastPropulsion = 0;

public:
	CommandInputSurface()
		: ITrackedSurfaceNotify()
	{
	}

	//Fast changing values, we cache their last value to avoid resending the same stuff.
	void SetDirection(const uint16_t value)
	{
		if (LastDirection != value)
		{
			LastDirection = value;
			Set16(SYNC_COMMAND_INPUT_DIRECTION_AND_PROPULSION_BLOCK_INDEX, value, SYNC_COMMAND_INPUT_DIRECTION_DATA_OFFSET);
		}
	}

	void SetPropulsion(const uint16_t value)
	{
		if (LastPropulsion != value)
		{
			LastPropulsion = value;
			Set16(SYNC_COMMAND_INPUT_DIRECTION_AND_PROPULSION_BLOCK_INDEX, value, SYNC_COMMAND_INPUT_PROPULSION_DATA_OFFSET);
		}
	}

	void SetDirectionTrim(const uint16_t value)
	{
		Set16(SYNC_COMMAND_INPUT_TRIM_BLOCK_INDEX, value, SYNC_COMMAND_INPUT_DIRECTION_DATA_OFFSET);
	}

	void SetPropulsionTrim(const uint16_t value)
	{
		Set16(SYNC_COMMAND_INPUT_TRIM_BLOCK_INDEX, value, SYNC_COMMAND_INPUT_PROPULSION_DATA_OFFSET);
	}

	uint16_t GetDirection()
	{
		return Get16(SYNC_COMMAND_INPUT_DIRECTION_AND_PROPULSION_BLOCK_INDEX, SYNC_COMMAND_INPUT_DIRECTION_DATA_OFFSET);
	}

	uint16_t GetPropulsion()
	{
		return Get16(SYNC_COMMAND_INPUT_DIRECTION_AND_PROPULSION_BLOCK_INDEX, SYNC_COMMAND_INPUT_PROPULSION_DATA_OFFSET);
	}

	uint16_t GetDirectionTrim()
	{
		return Get16(SYNC_COMMAND_INPUT_TRIM_BLOCK_INDEX, SYNC_COMMAND_INPUT_DIRECTION_DATA_OFFSET);
	}

	uint16_t GetPropulsionTrim()
	{
		return Get16(SYNC_COMMAND_INPUT_TRIM_BLOCK_INDEX, SYNC_COMMAND_INPUT_PROPULSION_DATA_OFFSET);
	}

public:
	uint8_t * GetData() { return Data; }
	IBitTracker* GetTracker() { return &Tracker; }

#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("CommandInput Surface "));
		serial->print(F("|"));
		for (uint8_t i = 0; i < GetSize(); i++)
		{
			serial->print(GetData()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}
#endif 	
};
#endif

