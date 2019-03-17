// LoLaManagerRCTransmitter.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#include "ExampleControllerSurface.h"
#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\SyncSurfaceWriter.h>

#define CONTROLLER_SURFACE_BASE_HEADER (PACKET_DEFINITION_USER_HEADERS_START)

class HostManager : public LoLaManagerHost
{
private:
	ControllerSurface Controller;

	SyncSurfaceReader<CONTROLLER_SURFACE_BASE_HEADER> Reader;

protected:
	bool OnSetupServices()
	{
		return LoLaDriver->GetServices()->Add(&Reader);
	}

public:
	HostManager(Scheduler* servicesScheduler, Scheduler* driverScheduler, LoLaPacketDriver* loLa)
		: LoLaManagerHost(servicesScheduler, driverScheduler, loLa)
		, Reader(servicesScheduler, loLa, &Controller)
	{
	}

	ControllerSurface* GetControllerSurface()
	{
		return &Controller;
	}
};
#endif

