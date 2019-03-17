// LoLaManager.h

#ifndef _LOLAMANAGER_h
#define _LOLAMANAGER_h


#include <PacketDriver\LoLaPacketDriver.h>

#include <Services\ILoLaService.h>
#include <Services\Link\LoLaLinkHostService.h>
#include <Services\Link\LoLaLinkRemoteService.h>

#include <Services\LoLaServicesManager.h>


class LoLaManager
{
protected:
	LoLaPacketDriver * LoLaDriver = nullptr;

protected:
	virtual LoLaLinkService * GetLinkService() { return nullptr; }

	virtual bool OnSetupServices()
	{
		return true;
	}

protected:
	bool SetupServices()
	{		
		if (!GetLinkService()->SetServicesManager(LoLaDriver->GetServices()))
		{
			return false;
		}

		if (!LoLaDriver->GetServices()->Add(GetLinkService()))
		{
			return false;
		}

		if (!OnSetupServices())
		{
			return false;
		}

		return true;
	}

public:
	LoLaManager(LoLaPacketDriver* driver)
	{
		LoLaDriver = driver;
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return LoLaDriver->GetServices()->GetLinkInfo();
	}

	void Start()
	{		
		GetLinkService()->Enable();
	}

	void Stop()
	{
		LoLaDriver->Disable();
		GetLinkService()->Disable();
	}

	bool Setup()
	{
		if (SetupServices())
		{
			return LoLaDriver->Setup();
		}

		return false;
	}
};

class LoLaManagerHost : public LoLaManager
{
protected:
	//Host base services.
	LoLaLinkHostService LinkService;

public:
	LoLaManagerHost(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(scheduler, driver)
	{
	}

protected:
	LoLaLinkService * GetLinkService() { return &LinkService; }
};

class LoLaManagerRemote : public LoLaManager
{
protected:
	//Remote base services.
	LoLaLinkRemoteService LinkService;

public:
	LoLaManagerRemote(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(scheduler, driver)
	{
	}

protected:
	LoLaLinkService * GetLinkService() { return &LinkService; }
};
#endif

