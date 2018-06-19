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
	//LoLa packet manager.
	LoLaPacketDriver * LoLa = nullptr;


protected:
	virtual LoLaLinkService * GetLinkService() { return nullptr; }

	virtual bool OnSetupServices()
	{
		return true;
	}


protected:
	bool SetupServices()
	{
		if (!LoLa->GetServices()->Add(GetLinkService()))
		{
			return false;
		}

		if (!GetLinkService()->AddSubServices(LoLa->GetServices()))
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
	LoLaManager(LoLaPacketDriver* loLa)
	{
		LoLa = loLa;
	}

	bool Start()
	{
		GetLinkService()->Enable();
		LoLa->Enable();
		return true;

	}

	bool Stop()
	{
		LoLa->Disable();
		return LoLa->GetServices()->StopAll();
	}

	bool Setup()
	{
		if (SetupServices())
		{
			return LoLa->Setup();
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
	LoLaManagerHost(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManager(loLa)
		, LinkService(scheduler, loLa)
	{
	}

	LoLaLinkService * GetLinkService() { return &LinkService; }
};

class LoLaManagerRemote : public LoLaManager
{
protected:
	//Remote base services.
	LoLaLinkRemoteService LinkService;

public:
	LoLaManagerRemote(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManager(loLa)
		, LinkService(scheduler, loLa)
	{
	}

	LoLaLinkService * GetLinkService() { return &LinkService; }
};
#endif

