// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONREMOTESERVICE_h
#define _LOLACONNECTIONREMOTESERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionRemoteService : public LoLaConnectionService
{
private:
	enum RemoteStateAwaitingConnectionEnum
	{
		SearchingForBroadcast,
		GotBroadcast,
		SendingChallenge,
		ResponseOk,
		ResponseNotOk,
		StartingDiagnostics,
		MeasuringLatency,
	} HostState = SearchingForBroadcast;
public:
	LoLaConnectionRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
	}

private:
	
};

#endif

