// LoLaManagerRCReceiver.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#include <ExampleControllerSurface.h>
#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\SyncSurfaceWriter.h>

#define CONTROLLER_SURFACE_BASE_HEADER (PACKET_DEFINITION_PING_HEADER + 1)

class RemoteManager : public LoLaManagerRemote
{
private:
	ControllerSurface Controller;

	SyncSurfaceWriter Writer;

public:
	RemoteManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerRemote(scheduler, loLa)
		, Writer(scheduler, loLa, CONTROLLER_SURFACE_BASE_HEADER, &Controller)
	{
	}

	ControllerSurface* GetControllerSurface()
	{
		return &Controller;
	}
};
#endif