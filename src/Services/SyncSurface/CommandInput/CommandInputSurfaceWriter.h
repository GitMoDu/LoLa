// CommandInputSurfaceWriter.h

#ifndef _COMMANDINPUTSURFACEWRITER_h
#define _COMMANDINPUTSURFACEWRITER_h

#include <Services\SyncSurface\CommandInput\CommandInputSurface.h>
#include <Services\SyncSurface\SyncSurfaceWriter.h>

class CommandInputSurfaceWriter : public SyncSurfaceWriter
{
private:
	CommandInputSurface Surface;

public:
	CommandInputSurfaceWriter(Scheduler* scheduler, ILoLa* loLa)
		: SyncSurfaceWriter(scheduler, loLa, PACKET_DEFINITION_SYNC_COMMAND_INPUT_BASE_HEADER, &Surface)
	{
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("CommandInputSurface Writer"));
	}
#endif // DEBUG_LOLA
};
#endif

