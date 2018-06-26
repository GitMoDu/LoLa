// CommandInputSurfaceReader.h

#ifndef _COMMANDINPUTSURFACEREADER_h
#define _COMMANDINPUTSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\CommandInput\CommandInputSurface.h>


class CommandInputSurfaceReader : public SyncSurfaceReader
{
private:
	CommandInputSurface Surface;

public:
	CommandInputSurfaceReader(Scheduler* scheduler, ILoLa* loLa)
		: SyncSurfaceReader(scheduler, loLa, PACKET_DEFINITION_SYNC_COMMAND_INPUT_BASE_HEADER, &Surface)
	{
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("CommandInputSurface Reader"));
	}
#endif // DEBUG_LOLA
};
#endif