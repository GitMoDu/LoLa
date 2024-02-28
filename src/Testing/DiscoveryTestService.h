// DiscoveryTestService.h

#ifndef _DISCOVERY_TEST_SERVICE_h
#define _DISCOVERY_TEST_SERVICE_h

#include "Services\Discovery\AbstractDiscoveryService.h"

template<const char OnwerName,
	const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize = 3>
class DiscoveryTestService : public AbstractDiscoveryService<Port, ServiceId>
{
private:
	using BaseClass = AbstractDiscoveryService<Port, ServiceId>;

public:
	DiscoveryTestService(Scheduler& scheduler, ILoLaLink* link)
		: BaseClass(scheduler, link)
	{}

private:
#ifdef DEBUG_LOLA
	void PrintName()
	{
		Serial.print(millis());
		Serial.print('\t');
		Serial.print('[');
		Serial.print(OnwerName);
		Serial.print(']');
		Serial.print('\t');
	}
#endif

protected:
	virtual void OnServiceStarted() final
	{
		Task::enableDelayed(0);
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery Active."));
#endif
	}

	virtual void OnDiscoveryFailed() final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery Failed."));
#endif
	}

	virtual void OnServiceEnded() final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery Inactive."));
#endif
	}

	virtual void OnLinkedServiceRun() final
	{
		Task::disable();
	}

	virtual void OnLinkedSendRequestFail() final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery OnLinkedSendRequestFail."));
#endif
	}

	virtual void OnLinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.print(F("Test Discovery OnLinkedPacketReceived ("));
		Serial.print(F("0x"));
		Serial.print(payload[HeaderDefinition::HEADER_INDEX]);
		Serial.println(F(")."));
#endif
	}
};
#endif