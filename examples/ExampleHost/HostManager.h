// LoLaManagerRCTransmitter.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#include <ExampleControllerSurface.h>
#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\SyncSurfaceWriter.h>

#define CONTROLLER_SURFACE_BASE_HEADER (PACKET_DEFINITION_USER_HEADERS_START + 1)

class HostManager : public LoLaManagerHost
{
private:
	ControllerSurface Controller;

	SyncSurfaceReader Reader;

protected:
	bool OnSetupServices()
	{
		return LoLa->GetServices()->Add(&Reader);
	}

public:
	HostManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerHost(scheduler, loLa)
		, Reader(scheduler, loLa, CONTROLLER_SURFACE_BASE_HEADER, &Controller)
	{
	}

	ControllerSurface* GetControllerSurface()
	{
		return &Controller;
	}
};
#endif

