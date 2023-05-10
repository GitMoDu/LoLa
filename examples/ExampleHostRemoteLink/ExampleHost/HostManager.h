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
	//ExampleControllerSurface Controller;

	//SyncSurfaceReader<ExampleControllerSurface::Port> ReaderController;

protected:
	bool OnSetupServices()
	{
		// Controller.
		//if (!ReaderController.Setup())
		//{
		//	return false;
		//}

		return true;
	}

public:
	HostManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerHost(scheduler, loLa)
		//, ReaderController(scheduler, loLa, &Controller)
	{
	}

	//ExampleControllerSurface* GetControllerSurface()
	//{
	//	return &Controller;
	//}
};
#endif

