// LoLaManagerRCReceiver.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#if defined(ARDUINO_ARCH_AVR)//Poor old ATmega's 2k of RAM is not enough.
#undef USE_TELEMETRY
#endif

#include <Services\SyncSurface\CommandInput\CommandInputSurfaceReader.h>
#ifdef USE_TELEMETRY
#include <Services\SyncSurface\Telemetry\TelemetrySurfaceWriter.h>
#endif

class RemoteManager : public LoLaManagerRemote
{
protected:
	CommandInputSurfaceReader CommandInput;
#ifdef USE_TELEMETRY
	TelemetrySurfaceWriter Telemetry;
#endif

protected:
	virtual bool OnSetupServices()
	{
		if (!LoLa->GetServices()->Add(&CommandInput))
		{
			return false;
		}

#ifdef USE_TELEMETRY
		if (!LoLa->GetServices()->Add(&Telemetry))
		{
			return false;
		}
#endif

		return true;
	}

public:
	RemoteManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerRemote(scheduler, loLa)
		, CommandInput(scheduler, loLa)
#ifdef USE_TELEMETRY
		, Telemetry(scheduler, loLa)
#endif
	{
	}


	CommandInputSurface* GetCommandInputSurface()
	{
		return (CommandInputSurface*)CommandInput.GetSurface();
	}
};
#endif

