// LoLaManagerRCTransmitter.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManager.h>

#include <Services\SyncSurface\CommandInput\CommandInputSurfaceWriter.h>
#include <Services\SyncSurface\Telemetry\TelemetrySurfaceReader.h>

class HostManager : public LoLaManagerHost
{
protected:
	CommandInputSurfaceWriter* CommandInput;
	TelemetrySurfaceReader* Telemetry;

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
	HostManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerHost(scheduler, loLa)
	{
		CommandInput = new CommandInputSurfaceWriter(scheduler, loLa);
		Telemetry = new TelemetrySurfaceReader(scheduler, loLa);
	}

	CommandInputSurface* GetCommandInputSurface()
	{
		return (CommandInputSurface*)CommandInput->GetSurface();//TODO: Make sure this cast does not cause problems.
	}
};


#endif

