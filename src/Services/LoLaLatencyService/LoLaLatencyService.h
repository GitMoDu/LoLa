// LoLaLatencyService.h

#ifndef _LOLA_LATENCY_SERVICE_h
#define _LOLA_LATENCY_SERVICE_h

#include <Services\IPacketSendService.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <RingBufCPP.h>
#include <Callback.h>


#define LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE						3
#define LOLA_LATENCY_PING_DATA_MAX_DEVIATION_SIGMA					((float)0.1 + ((float)LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE/(float)50))

//Max value UINT8_MAX
#define LOLA_LATENCY_SERVICE_MAX_CANCEL_COUNT						25

//65536 is the max uint16_t, about 65 ms max latency is accepted.
#define LOLA_LATENCY_SERVICE_PING_TIMEOUT_MILLIS					65
#define LOLA_LATENCY_SERVICE_PING_TIMEOUT_MICROS					(LOLA_LATENCY_SERVICE_PING_TIMEOUT_MILLIS*1000)

#define LOLA_LATENCY_SERVICE_POLL_PERIOD_MILLIS						1000
#define LOLA_LATENCY_SERVICE_BACK_OFF_DURATION_MILLIS				1000

#define LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS			40
#define LOLA_LATENCY_SERVICE_SEND_FAILED_BACK_OFF_DURATION_MILLIS	80

#define LOLA_LATENCY_SERVICE_NO_REPLY_TIMEOUT_MILLIS				(LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE*(100 + LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS+LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS))
#define LOLA_LATENCY_SERVICE_UNABLE_TO_MEASURE_TIMEOUT_MILLIS		((uint32_t)5000 + (uint32_t)LOLA_LATENCY_SERVICE_NO_REPLY_TIMEOUT_MILLIS)

#define PACKET_DEFINITION_PING_HEADER								(PACKET_DEFINITION_LINK_ACK_HEADER+1)
#define PACKET_DEFINITION_PING_PAYLOAD_SIZE							0

class LoLaLatencyService : public IPacketSendService
{
private:
	class PingPacketDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_PING_HEADER; }
		uint8_t GetPayloadSize() { return PACKET_DEFINITION_PING_PAYLOAD_SIZE; }
	} PingDefinition;

	enum LatencyServiceStateEnum
	{
		Setup,
		Starting,
		Checking,
		Sending,
		BackOff,
		AnalysingResults,
		Done
	} State = LatencyServiceStateEnum::Done;

	LoLaPacketNoPayload PacketHolder;//Optimized memory usage grunt packet.

	uint32_t MeasurementStart = ILOLA_INVALID_MILLIS;
	uint32_t LastSentTimeStamp = ILOLA_INVALID_MILLIS;
	uint16_t StartUpDelay = 0;
	volatile uint8_t SentId;

	RingBufCPP<uint16_t , PROCESS_EVENT_QUEUE_MAX_QUEUE_DEPTH> DurationStack;

	uint16_t SampleDuration = ILOLA_INVALID_LATENCY;
	uint32_t DurationSum = ILOLA_INVALID_MILLIS;

	uint8_t SamplesCancelled = 0;

	//Callback handler
	Signal<const bool> MeasurementCompleteEvent;

public:
	LoLaLatencyService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LATENCY_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
	{
	}

	float GetLatency()
	{
		return ((float)GetRTT() / (float)2000);
	}

	uint16_t GetRTT()
	{
		return GetAverage();
	}

	void RequestRefreshPing(const uint16_t startUpDelay = 0)
	{
		Enable();
		StartUpDelay = startUpDelay;
		State = LatencyServiceStateEnum::Setup;
	}

	void RequestSinglePing()
	{
		PreparePacket();
		RequestSendPacket();
		State = LatencyServiceStateEnum::Done;
		Enable();
	}

	void Stop()
	{
		State = LatencyServiceStateEnum::Done;
		SetNextRunDefault();
		Disable();
	}

	void SetMeasurementCompleteCallback(const Slot<const bool>& slot)
	{
		MeasurementCompleteEvent.attach(slot);
	}

private:
	uint16_t GetAverage()
	{
		if (DurationStack.numElements() > LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE - 1)
		{
			DurationSum = 0;
			for (uint8_t i = 0; i < DurationStack.numElements(); i++)
			{
				DurationSum += *DurationStack.peek(i);
			}

			//Average is temporarily stored in DurationSum.
			DurationSum = DurationSum / DurationStack.numElements();

			uint16_t MaxDeviation = ceil((float)DurationSum)*(LOLA_LATENCY_PING_DATA_MAX_DEVIATION_SIGMA);
			for (uint8_t i = 0; i < DurationStack.numElements(); i++)
			{
				if (abs((int32_t)*DurationStack.peek(i) - DurationSum) > MaxDeviation)
				{
					return ILOLA_INVALID_LATENCY;
				}
			}

			//Value is always smaller than uint16, because samples with higher value are filtered out on acquisition.
			return (uint16_t)DurationSum;
		}
		else
		{
			return ILOLA_INVALID_LATENCY;
		}
	}

	void ClearDurations()
	{
		while (!DurationStack.isEmpty())
		{
			DurationStack.pull();
		}
		SamplesCancelled = 0;
		LastSentTimeStamp = ILOLA_INVALID_MILLIS;
	}

	void PreparePacket()
	{
		SentId = random(0xFF);
		PacketHolder.SetDefinition(&PingDefinition);
		PacketHolder.SetId(SentId);
	}

	void CancelSample()
	{		
		if (State == LatencyServiceStateEnum::Sending)
		{
			ClearSendRequest();
			LastSentTimeStamp = ILOLA_INVALID_MILLIS;
			SamplesCancelled++;
			SetNextRunASAP();
		}
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Latency Diagnostics"));
	}
#endif // DEBUG_LOLA

	bool OnEnable()
	{
		return true;
	}

	void OnDisable()
	{
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		return packetMap->AddMapping(&PingDefinition);
	}

	bool ShouldRespondToOutsidePackets()
	{
		return IsSetupOk() && (State != LatencyServiceStateEnum::Done);
	}

	void SetNextRunDelayRandom(const uint16_t range)
	{
		SetNextRunDelay(range / 2 + random(range / 2));
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_PING_HEADER)
		{
			return true;
		}
		return false;
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		SampleDuration = Micros() - LastSentTimeStamp;
		if (ShouldRespondToOutsidePackets() &&
			header == PACKET_DEFINITION_PING_HEADER &&
			State == LatencyServiceStateEnum::Sending)
		{
			if (SentId == id)
			{
				if (LastSentTimeStamp != ILOLA_INVALID_MILLIS &&
					SampleDuration < LOLA_LATENCY_SERVICE_PING_TIMEOUT_MICROS)
				{
					DurationStack.addForce(SampleDuration);
				}
				State = LatencyServiceStateEnum::BackOff;
				SetNextRunASAP();
			}
		}
	}

	bool OnSetup()
	{
		if (!IPacketSendService::OnSetup())
		{
			return false;
		}

		Stop();
		return true;
	}

	void OnSendOk()
	{
		SetNextRunASAP();
	}

	void OnSendDelayed()
	{
		CancelSample();
	}

	void OnSendRetrying()
	{
		CancelSample();
	}

	void OnSendFailed()
	{
		CancelSample();
	}

	void OnSendTimedOut()
	{
		CancelSample();
	}

	void OnAckFailed(const uint8_t header, const uint8_t id) 
	{
		CancelSample();
	}

	void OnService()
	{
		switch (State)
		{
		case LatencyServiceStateEnum::Setup:
			ClearDurations();
			
			//TimeOutPointMillis = Millis() + LOLA_LATENCY_SERVICE_UNABLE_TO_MEASURE_TIMEOUT_MILLIS;
			State = LatencyServiceStateEnum::Starting;
			SetNextRunDelay(StartUpDelay);
			break;
		case LatencyServiceStateEnum::Starting:
			MeasurementStart = Millis();
#ifdef DEBUG_LOLA
			Serial.println(F("Measuring Latency..."));
#endif
			ClearDurations();
#ifdef MOCK_RADIO
#ifdef DEBUG_LOLA
			Serial.println(F("Mock Latency done."));
#endif
			State = LatencyServiceStateEnum::Done;
			MeasurementCompleteEvent.fire(true);
#else
			State = LatencyServiceStateEnum::Checking;
#endif
			SetNextRunASAP();
			break;
		case LatencyServiceStateEnum::Checking:
			//Have we timed out for good?
			if (MeasurementStart == ILOLA_INVALID_MILLIS ||
				(Millis() - MeasurementStart > LOLA_LATENCY_SERVICE_UNABLE_TO_MEASURE_TIMEOUT_MILLIS))
			{
				ClearDurations();
				State = LatencyServiceStateEnum::Done;
				SetNextRunASAP();
#ifdef MOCK_RADIO
				MeasurementCompleteEvent.fire(true);
#else
#ifdef DEBUG_LOLA
				Serial.println(F("Timed out."));
#endif	
				MeasurementCompleteEvent.fire(false);
#endif
			}
			//Is the data link too busy to get accurate results?
			else if (SamplesCancelled > LOLA_LATENCY_SERVICE_MAX_CANCEL_COUNT)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Cancelled out: "));
				Serial.print(SamplesCancelled);
				Serial.println(F(" times."));
#endif	
				State = LatencyServiceStateEnum::Checking;
				ClearDurations();
				SetNextRunASAP();
			}
			//Do we have needed sample count?
			else if (DurationStack.numElements() >= LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE)
			{
				State = LatencyServiceStateEnum::AnalysingResults;
				SetNextRunASAP();
			}
			//All right, lets send a packet and get a sample!
			else
			{
				State = LatencyServiceStateEnum::Sending;
				PreparePacket();
				RequestSendPacket((uint8_t)(LOLA_LATENCY_SERVICE_PING_TIMEOUT_MICROS / 1000));
				LastSentTimeStamp = Micros();
			}
			break;
		case LatencyServiceStateEnum::Sending:
			//If we're here, it means the ack failed to arrive.
			State = LatencyServiceStateEnum::Checking;
			SetNextRunDelayRandom(LOLA_LATENCY_SERVICE_SEND_FAILED_BACK_OFF_DURATION_MILLIS);
			break;
		case LatencyServiceStateEnum::BackOff:
			//If we're here, it means the ack arrived and was valid.
			LastSentTimeStamp = ILOLA_INVALID_MILLIS; //Make sure we ignore stale acks.
			State = LatencyServiceStateEnum::Checking;
			//Do we have needed sample count?
			if (DurationStack.numElements() >= LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE)
			{
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelay(LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS);
			}
			break;
		case LatencyServiceStateEnum::AnalysingResults:
			if (GetAverage() != ILOLA_INVALID_LATENCY)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Took "));
				Serial.print(Millis() - MeasurementStart);
				Serial.println(F(" ms"));
#endif
				State = LatencyServiceStateEnum::Done;
				SetNextRunASAP();
				MeasurementCompleteEvent.fire(true);
			}
			else
			{
				DurationStack.pull();
				LastSentTimeStamp = ILOLA_INVALID_MILLIS;
				State = LatencyServiceStateEnum::Checking;
				SetNextRunASAP();
			}
			break;
		case LatencyServiceStateEnum::Done:
		default:
			Disable();
			break;
		}
	}
};
#endif