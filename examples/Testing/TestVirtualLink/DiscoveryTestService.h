// DiscoveryTestService.h

#ifndef _DISCOVERY_TEST_SERVICE_h
#define _DISCOVERY_TEST_SERVICE_h

#include "LoLaServices\AbstractLoLaDiscoveryService.h"

template<const char OnwerName,
	const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize = 3>
class DiscoveryTestService : public AbstractLoLaDiscoveryService<Port, ServiceId>
{
private:
	using BaseClass = AbstractLoLaDiscoveryService<Port, ServiceId>;

public:
	DiscoveryTestService(Scheduler& scheduler, ILoLaLink* link)
		: BaseClass(scheduler, link)
	{}

private:
	void PrintName()
	{
#ifdef DEBUG_LOLA
		Serial.print('{');
		Serial.print(OnwerName);
		Serial.print('}');
		Serial.print('\t');
#endif
	}

protected:
	/// <summary>
	/// Fires when the the service has been discovered.
	/// </summary>
	virtual void OnServiceStarted()
	{
#if defined(PRINT_DISCOVERY)
		PrintName();
		Serial.println(F("Test Discovery Service Started."));
#endif
	}

	/// <summary>
	/// Fires when the the service partner can't be found.
	/// </summary>
	virtual void OnDiscoveryFailed() final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery Failed."));
#endif
		Task::disable();
	}

	/// <summary>
	/// Fires when the the service has been lost.
	/// </summary>
	virtual void OnServiceEnded()
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery Service Ended."));
#endif
		Task::disable();
	}

	/// <summary>
	/// The user class can ride this task's callback while linked,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnLinkedService()
	{
		Task::disable();
	}

	/// <summary>
	/// Fires when the user class's packet send request
	///  failed after transmission request.
	/// </summary>
	virtual void OnLinkedSendRequestFail()
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Discovery OnLinkedSendRequestFail."));
#endif
	}

	/// <summary>
	/// After discovery is completed, any non-discovery packets will be routed to the user class.
	/// </summary>
	/// <param name="startTimestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	virtual void OnLinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize)
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