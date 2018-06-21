// LoLaDiagnosticsService.h

#ifndef _LOLA_DIAGNOSTICS_SERVICE_h
#define _LOLA_DIAGNOSTICS_SERVICE_h

#include <LoLaLinkInfo.h>
#include <Services\ILoLaService.h>
#include <Services\LoLaLatencyService\LoLaLatencyService.h>


#define LOLA_DIAGNOSTICS_SERVICE_POLL_PERIOD_MILLIS				500

class LoLaDiagnosticsService : public ILoLaService
{
public:
	LoLaDiagnosticsService(Scheduler* scheduler, ILoLa* loLa)
		: ILoLaService(scheduler, LOLA_DIAGNOSTICS_SERVICE_POLL_PERIOD_MILLIS, loLa)
		, LatencyService(scheduler, loLa)
	{
		AttachCallbacks();
	}

	bool AddSubServices(LoLaServicesManager * servicesManager)
	{
		if (servicesManager == nullptr || !servicesManager->Add(&LatencyService))
		{
			return false;
		}

		SetNextRunDefault();
		return true;
	}

private:
	//Subservices.
	LoLaLatencyService LatencyService;

	LoLaLinkInfo* LinkInfo = nullptr;

protected:


private:
	void OnLatencyMeasurementComplete(const bool success)
	{
		if (LinkInfo != nullptr)
		{
			if (success)
			{

				LinkInfo->SetRTT(LatencyService.GetRTT());
			}
			else
			{
				LinkInfo->ResetLatency();
			}
		}	
	}

	void AttachCallbacks()
	{
		MethodSlot<LoLaDiagnosticsService, const bool> memFunSlot(this, &LoLaDiagnosticsService::OnLatencyMeasurementComplete);
		LatencyService.SetMeasurementCompleteCallback(memFunSlot);
	}
};
#endif

