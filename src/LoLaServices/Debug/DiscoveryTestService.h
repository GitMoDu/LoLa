// DiscoveryTestService.h

#ifndef _DISCOVERY_TEST_SERVICE_h
#define _DISCOVERY_TEST_SERVICE_h


#include "..\..\Services\AbstractLoLaDiscoveryService.h"
#include "Stream.h"

template<const uint8_t Port, const uint8_t ServiceId>
class DiscoveryTestService
	: public AbstractLoLaDiscoveryService<Port, 1>
{
private:
	using BaseClass = AbstractLoLaDiscoveryService<Port, 1>;

	using SendResultEnum = ILinkPacketSender::SendResultEnum;

protected:
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetLastSent;
	using BaseClass::OutPacket;

private:

	Stream* SerialOut;

public:
	DiscoveryTestService(Scheduler& scheduler, ILoLaLink* link, Stream* serial = nullptr)
		: BaseClass(scheduler, link)
		, SerialOut(serial)
	{
	}

public:
	virtual void OnServiceStarted() final
	{
		if (SerialOut != nullptr)
		{
			SerialOut->print(F("Test Service "));
			SerialOut->print(ServiceId);
			SerialOut->println(F(" started."));
		}
	}

	virtual void OnServiceEnded() final
	{
		SerialOut->print(F("Test Service "));
		SerialOut->print(ServiceId);
		SerialOut->println(F(" stopped."));
	}

protected:
	virtual const uint8_t GetServiceId() final
	{
		return ServiceId;
	}
};
#endif