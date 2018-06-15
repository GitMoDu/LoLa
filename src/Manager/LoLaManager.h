// LoLaManager.h

#ifndef _LOLAMANAGER_h
#define _LOLAMANAGER_h


#include <PacketDriver\LoLaPacketDriver.h>

#include <Services\ILoLaService.h>
#include <Services\Connection\LoLaConnectionHostService.h>
#include <Services\Connection\LoLaConnectionRemoteService.h>

#include <Services\LoLaServicesManager.h>

class LoLaManager
{
protected:
	//LoLa packet manager.
	LoLaPacketDriver * LoLa = nullptr;


protected:
	virtual LoLaConnectionService * GetConnectionService() { return nullptr; }

	virtual bool OnSetupServices()
	{
		return true;
	}


protected:
	bool SetupServices()
	{
		if (!LoLa->GetServices()->Add(GetConnectionService()))
		{
			return false;
		}

		if (!GetConnectionService()->AddSubServices(LoLa->GetServices()))
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
		GetConnectionService()->Enable();
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
	LoLaConnectionHostService ConnectionService;

public:
	LoLaManagerHost(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManager(loLa)
		, ConnectionService(scheduler, loLa)
	{
	}

	LoLaConnectionService * GetConnectionService() { return &ConnectionService; }
};

class LoLaManagerRemote : public LoLaManager
{
protected:
	//Remote base services.
	LoLaConnectionRemoteService ConnectionService;

public:
	LoLaManagerRemote(Scheduler* scheduler, LoLaPacketDriver* loLa)
		: LoLaManager(loLa)
		, ConnectionService(scheduler, loLa)
	{
	}

	LoLaConnectionService * GetConnectionService() { return &ConnectionService; }
};

#endif

