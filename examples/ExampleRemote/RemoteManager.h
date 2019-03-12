// LoLaManagerRCReceiver.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#include <ExampleControllerSurface.h>
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
		return LoLa->GetServices()->Add(&Writer);
	}

public:
	RemoteManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerRemote(scheduler, loLa)
		, Writer(scheduler, loLa, &Controller)
	{
	}

	ControllerSurface* GetControllerSurface()
	{
		return &Controller;
	}
};
#endif