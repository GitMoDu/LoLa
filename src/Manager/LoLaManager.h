// LoLaManager.h

#ifndef _LOLAMANAGER_h
#define _LOLAMANAGER_h


#include <PacketDriver\LoLaPacketDriver.h>
#include <Link\LoLaLinkHostService.h>
#include <Link\LoLaLinkRemoteService.h>


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

	void Start()
	{
		GetLinkService()->Start();
	}

	void Stop()
	{
		GetLinkService()->Stop();
	}

	bool Setup()
	{
		if (GetLinkService() == nullptr || !GetLinkService()->Setup())
		{
			Serial.println(F("GetLinkService Setup Error"));

			return false;
		}

		if (!OnSetupServices())
		{
			Serial.println(F("Services Setup Error"));

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
	LoLaManagerHost(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(scheduler, driver)
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
	LoLaManagerRemote(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaManager(driver)
		, LinkService(scheduler, driver)
	{
	}

protected:
	LoLaLinkService* GetLinkService() { return &LinkService; }
};
#endif

