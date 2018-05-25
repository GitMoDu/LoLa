// TelemetrySurfaceWriter.h

#ifndef _TELEMETRYSURFACEWRITER_h
#define _TELEMETRYSURFACEWRITER_h

#include <Services\SyncSurface\SyncSurfaceWriter.h>
#include <Services\SyncSurface\Telemetry\TelemetrySurface.h>

class TelemetrySurfaceWriter : public SyncSurfaceWriter
{
private:
	TelemetrySurface Surface;

public:
	TelemetrySurfaceWriter(Scheduler* scheduler, ILoLa* loLa)
		: SyncSurfaceWriter(scheduler, loLa, PACKET_DEFINITION_SYNC_TELEMETRY_BASE_HEADER, &Surface)
	{
	}

	ITrackedSurface* GetSurface()
	{
		return &Surface;
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("TelemetrySurface Writer"));
	}
#endif
};

#endif

