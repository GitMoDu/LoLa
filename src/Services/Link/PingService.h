// PingService.h

#ifndef _PINGSERVICE_h
#define _PINGSERVICE_h


#include <Services\IPacketSendService.h>


//Latency measurement.
class PingService : public IPacketSendService
{
private:
	PingPacketDefinition DefinitionPing;

	const static uint8_t SAMPLE_COUNT = LATENCY_SERVICE_MAX_LATENCY_SAMPLES;
	const static uint32_t RESEND_PERIOD = ILOLA_DUPLEX_PERIOD_MILLIS * 2;
	const static uint32_t PING_TIMEOUT_MILLIS = ILOLA_DUPLEX_PERIOD_MILLIS * 4;
	const static uint16_t MAX_RTT_MICROS = ILOLA_DUPLEX_PERIOD_MILLIS * 1000;

	// Only payload is Id.
	TemplateLoLaPacket<LOLA_PACKET_MIN_PACKET_SIZE> OutPacket;

	enum PingStatusEnum : uint8_t
	{
		Clear = 0,
		Sent = 1
	};
	uint32_t Timeout = 0;

	PingStatusEnum PingStatus = PingStatusEnum::Clear;

	uint32_t LastSentMicros = 0;

	uint32_t Grunt = 0;

	bool RTTSet = false;

	//TODO: Store more rich timing information, instead of just the broad duration.
	RingBufCPP<uint16_t, SAMPLE_COUNT> DurationStack;

public:
	PingService(Scheduler* scheduler, ILoLaDriver* driver)
		: IPacketSendService(scheduler, 0, driver, &OutPacket)
		, DefinitionPing(this)
	{
	}

	void Start()
	{
		SetNextRunDelay(1);
	}

	bool HasRTT()
	{
		return RTTSet;
	}

	bool PiggybackSendPacket(ILoLaPacket* packet)
	{
		OutPacket.SetDefinition(packet->GetDefinition());

		for (uint8_t i = 0; i < packet->GetDefinition()->GetTotalSize(); i++)
		{
			OutPacket.GetRaw()[i] = packet->GetRaw()[i];
		}
		RequestSendPacket();
	}

	virtual bool Callback()
	{
		if (DurationStack.numElements() < SAMPLE_COUNT)
		{
			switch (PingStatus)
			{
			case PingStatusEnum::Clear:
				PreparePing();
				RequestSendPacket();
				Timeout = millis() + PING_TIMEOUT_MILLIS;
				PingStatus = PingStatusEnum::Sent;
				break;
			case PingStatusEnum::Sent:
				if (millis() > Timeout)
				{
#ifdef DEBUG_LOLA
					Serial.println("Latency meter timed out.");
#endif
					Disable();
				}
				break;
			default:
				PingStatus = PingStatusEnum::Clear;
				SetNextRunASAP();
				break;
			}
		}
		else
		{
			RTTSet = true;

			// Job done.
			Disable();
		}
	}

	virtual bool Setup()
	{
		if (LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionPing))
		{
			return false;
		}

		if (Packet->GetMaxSize() < DefinitionPing.GetTotalSize())
		{
			return false;
		}

		return IPacketSendService::Setup();
	}

	virtual void OnAckOk(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (PingStatus == PingStatusEnum::Sent)
		{
			PingStatus == PingStatusEnum::Clear;

			if (OutPacket.GetId() == id)
			{
				Grunt = timestamp - LastSentMicros;
				if (Grunt < MAX_RTT_MICROS)
				{
					DurationStack.addForce((uint16_t)Grunt);
				}
				else
				{
					// Outlier value discarded.
				}
			}
			SetNextRunASAP();
		}
	}

	bool OnPacketReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp, uint8_t* payload)
	{
		return ShouldProcessReceived();
	}

	void Reset()
	{
		RTTSet = false;
		LastSentMicros = 0;
		while (!DurationStack.isEmpty())
		{
			DurationStack.pull();
		}
	}

protected:
	void OnPreSend()
	{
		LastSentMicros = micros();
	}

private:
	uint16_t GetAverageLatency()
	{
		uint64_t DurationSum;

		// First pass, get average.
		DurationSum = 0;
		for (uint8_t i = 0; i < DurationStack.numElements(); i++)
		{
			DurationSum += *DurationStack.peek(i);
		}

		uint32_t Average = DurationSum / DurationStack.numElements();

		// Second pass, get variance.
		DurationSum = 0;
		uint8_t GoodCounter = 0;
		for (uint8_t i = 0; i < DurationStack.numElements(); i++)
		{
			if (abs(((int32_t)*DurationStack.peek(i)) - (int32_t)Average) < 100)
			{
				DurationSum += *DurationStack.peek(i);
			}
		}

		//Value is always smaller than uint16, because samples with higher value are filtered out on acquisition.
		return (DurationSum / DurationStack.numElements());
	}

	void PreparePing()
	{
		OutPacket.SetDefinition(&DefinitionPing);
		OutPacket.SetId(random(0, UINT8_MAX));
	}
};
#endif