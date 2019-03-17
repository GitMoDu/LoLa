// LoLaManagerRCReceiver.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#include "ExampleControllerSurface.h"
#include <Services\SyncSurface\SyncSurfaceReader.h>
#include <Services\SyncSurface\SyncSurfaceWriter.h>

#define CONTROLLER_SURFACE_BASE_HEADER (PACKET_DEFINITION_USER_HEADERS_START)

class RemoteManager : public LoLaManagerRemote
{
private:
	ControllerSurface Controller;

	SyncSurfaceWriter<CONTROLLER_SURFACE_BASE_HEADER> Writer;

protected:
	bool OnSetupServices()
	{
		return LoLaDriver->GetServices()->Add(&Writer);
	}

public:
	RemoteManager(Scheduler* servicesScheduler, Scheduler* driverScheduler, LoLaPacketDriver* loLa)
		: LoLaManagerRemote(servicesScheduler, driverScheduler, loLa)
		, Writer(servicesScheduler, loLa, &Controller)
	{
	}

	ControllerSurface* GetControllerSurface()
	{
		return &Controller;
	}
};
#endif