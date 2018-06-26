// TelemetrySurfaceReader.h

#ifndef _TELEMETRYSURFACEREADER_h
#define _TELEMETRYSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\Telemetry\TelemetrySurface.h>


class TelemetrySurfaceReader : public SyncSurfaceReader
{
private:
	TelemetrySurface Surface;

public:
	TelemetrySurfaceReader(Scheduler* scheduler, ILoLa* loLa)
		: SyncSurfaceReader(scheduler, loLa, PACKET_DEFINITION_SYNC_TELEMETRY_BASE_HEADER, &Surface)
	{
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("TelemetrySurface Reader"));
	}
#endif // DEBUG_LOLA
};
#endif
