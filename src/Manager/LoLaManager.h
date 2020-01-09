// LoLaManager.h

#ifndef _LOLAMANAGER_h
#define _LOLAMANAGER_h


#include <PacketDriver\LoLaPacketDriver.h>

#include <Services\ILoLaService.h>
#include <Services\Link\LoLaLinkHostService.h>
#include <Services\Link\LoLaLinkRemoteService.h>

#include <PacketDriver\LinkIndicator.h>


class LoLaManager
{
protected:
	LoLaPacketDriver* LoLaDriver = nullptr;

protected:
	virtual LoLaLinkService* GetLinkService() { return nullptr; }

	virtual bool OnSetupServices()
	{
		return true;
	}

public:
	LoLaManager(LoLaPacketDriver* driver)
	{
		LoLaDriver = driver;
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return GetLinkService()->GetLinkInfo();
	}

	LinkIndicator* GetLinkIndicator()
	{
		return GetLinkService()->GetLinkIndicator();
	}

#ifdef DEBUG_LOLA
	uint8_t GetLinkState()
	{
		return GetLinkService()->GetLinkState();
	}
#endif

	void Start()
	{
		GetLinkService()->Enable();
	}

	void Stop()
	{
		GetLinkService()->Disable();
	}

	bool Setup()
	{
		if (!(GetLinkService() != nullptr) || !GetLinkService()->Setup())
		{
			return false;
		}

		if (!OnSetupServices())
		{
			return false;
		}

		return LoLaDriver->Setup();
	}
};

class LoLaManagerHost : public LoLaManager
{
protected:
	//Host base services.
	LoLaLinkHostService LinkService;

public:
	LoLaManagerHost(Scheduler* servicesScheduler, Scheduler* driverScheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(servicesScheduler, driverScheduler, driver)
	{
	}

protected:
	LoLaLinkService* GetLinkService() { return &LinkService; }
};

class LoLaManagerRemote : public LoLaManager
{
protected:
	//Remote base services.
	LoLaLinkRemoteService LinkService;

public:
	LoLaManagerRemote(Scheduler* servicesScheduler, Scheduler* driverScheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(servicesScheduler, driverScheduler, driver)
	{
	}

protected:
	LoLaLinkService* GetLinkService() { return &LinkService; }
};
#endif

