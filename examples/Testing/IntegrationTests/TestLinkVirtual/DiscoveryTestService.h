// DiscoveryTestService.h

#ifndef _DISCOVERY_TEST_SERVICE_h
#define _DISCOVERY_TEST_SERVICE_h

#include "LoLaServices\AbstractLoLaDiscoveryService.h"

template<const uint8_t Port,
	const char OnwerName>
class DiscoveryTestService : public AbstractLoLaDiscoveryService<OnwerName, Port>
{
private:
	using BaseClass = AbstractLoLaDiscoveryService<OnwerName, Port>;

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
		Serial.println(F("OnServiceStarted."));
#endif
	}

	/// <summary>
	/// Fires when the the service has been lost.
	/// </summary>
	virtual void OnServiceEnded()
	{
#if defined(PRINT_DISCOVERY)
		PrintName();
		Serial.println(F("OnServiceEnded."));
#endif
		Task::disable();
	}

	/// <summary>
	/// The user class can ride this task's callback while linked,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnLinkedService()
	{
#if defined(PRINT_DISCOVERY)
		PrintName();
		Serial.println(F("OnLinkedService."));
#endif
		Task::disable();
	}

	/// <summary>
	/// Fires when the user class's packet send request
	///  failed after transmission request.
	/// </summary>
	virtual void OnLinkedSendRequestFail()
	{
#if defined(PRINT_DISCOVERY)
		PrintName();
		Serial.println(F("OnLinkedSendRequestFail."));
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
#if defined(PRINT_DISCOVERY)
		PrintName();
		Serial.print(F("OnLinkedPacketReceived ("));
		Serial.print(F("0x"));
		Serial.print(payload[SubHeaderDefinition::SUB_HEADER_INDEX]);
		Serial.println(F(")."));
#endif
	}
};
#endif