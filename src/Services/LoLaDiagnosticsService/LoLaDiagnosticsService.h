// LoLaDiagnosticsService.h

#ifndef _LOLA_DIAGNOSTICS_SERVICE_h
#define _LOLA_DIAGNOSTICS_SERVICE_h

#include <LoLaLinkInfo.h>
#include <Services\ILoLaService.h>
#include <Services\LoLaLatencyService\LoLaLatencyService.h>


#define LOLA_DIAGNOSTICS_SERVICE_POLL_PERIOD_MILLIS				500

class LoLaDiagnosticsService : public ILoLaService
{
private:
	//Subservices.
	LoLaLatencyService LatencyService;

	LoLaLinkInfo* LinkInfo = nullptr;

public:
	LoLaDiagnosticsService(Scheduler* scheduler, ILoLa* loLa, LoLaLinkInfo* linkInfo)
		: ILoLaService(scheduler, LOLA_DIAGNOSTICS_SERVICE_POLL_PERIOD_MILLIS, loLa)
		, LatencyService(scheduler, loLa)
	{
		LinkInfo = linkInfo;
	}

	bool AddSubServices(LoLaServicesManager * servicesManager)
	{
		if (servicesManager == nullptr || !servicesManager->Add(&LatencyService))
		{
			return false;
		}

		AttachCallbacks();

		SetNextRunDefault();
		return true;
	}

	void OnLinkEstablished() 
	{
		LatencyService.RequestRefreshPing(500);
	}

	void OnLinkLost()
	{
		LatencyService.Stop();
	}


private:
	void AttachCallbacks()
	{
		MethodSlot<LoLaDiagnosticsService, const bool> memFunSlot(this, &LoLaDiagnosticsService::OnLatencyMeasurementComplete);
		LatencyService.SetMeasurementCompleteCallback(memFunSlot);
	}

protected:
	void OnLatencyMeasurementComplete(const bool success)
	{
		if (!LinkInfo->HasLink())
		{
			return;
		}
		if (LinkInfo != nullptr)
		{
			if (success)
			{
				LinkInfo->SetRTT(LatencyService.GetRTT());

#ifdef DEBUG_LOLA
				Serial.print(F("Latency: "));
				Serial.print(LatencyService.GetLatency(), 2);
				Serial.println(F(" ms"));
#endif
			}
			else
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Latency measurement failed."));
#endif
				LinkInfo->ResetLatency();
				LatencyService.RequestRefreshPing(2000);
			}
		}
	}
};
#endif

