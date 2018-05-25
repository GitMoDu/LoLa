// LoLaManagerRCReceiver.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManager.h>
#include <Services\SyncSurface\CommandInput\CommandInputSurfaceReader.h>
#include <Services\SyncSurface\Telemetry\TelemetrySurfaceWriter.h>

class RemoteManager : public LoLaManagerRemote
{
protected:
	CommandInputSurfaceReader* CommandInput;
	TelemetrySurfaceWriter* Telemetry;

protected:
	virtual bool OnSetupServices()
	{
		if (!LoLa->GetServices()->Add(CommandInput))
		{
			return false;
		}

		if (!LoLa->GetServices()->Add(Telemetry))
		{
			return false;
		}

		return true;
	}

public:
	RemoteManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerRemote(scheduler, loLa)
	{
		CommandInput = new CommandInputSurfaceReader(scheduler, loLa);
		Telemetry = new TelemetrySurfaceWriter(scheduler, loLa);
	}


	CommandInputSurface* GetCommandInputSurface()
	{
		return (CommandInputSurface*)CommandInput->GetSurface();//TODO: Make sure this cast does not cause problems.
	}
};


#endif

