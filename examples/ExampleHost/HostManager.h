// LoLaManagerRCTransmitter.h

#ifndef _LOLAMANAGERRCRECEIVER_h
#define _LOLAMANAGERRCRECEIVER_h

#include <LoLaManagerInclude.h>

#define USE_TELEMETRY

#if defined(ARDUINO_ARCH_AVR)//Poor old ATmega's 2k of RAM is not enough.
#undef USE_TELEMETRY
#endif

#include <Services\SyncSurface\CommandInput\CommandInputSurfaceWriter.h>
#ifdef USE_TELEMETRY
#include <Services\SyncSurface\Telemetry\TelemetrySurfaceReader.h>
#endif


class HostManager : public LoLaManagerHost
{
protected:
	CommandInputSurfaceWriter CommandInput;

#ifdef USE_TELEMETRY
	TelemetrySurfaceReader Telemetry;
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
	HostManager(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManagerHost(scheduler, loLa)
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

