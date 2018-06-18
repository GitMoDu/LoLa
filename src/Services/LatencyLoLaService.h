// LatencyLoLaService.h

#ifndef _LATENCYRADIOSERVICE_h
#define _LATENCYRADIOSERVICE_h

#include <Services\IPacketSendService.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <RingBufCPP.h>
#include <Callback.h>


#define LATENCY_PING_DATA_POINT_STACK_SIZE 5

//65536 is the max uint16_t, about 65 ms max latency is accepted.
#define LATENCY_SERVICE_PING_TIMEOUT_MICROS 65000

#define LOLA_LATENCY_SERVICE_POLL_PERIOD_MILLIS 50
#define LOLA_LATENCY_SERVICE_BACK_OFF_DURATION_MILLIS 1000

#define LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS 20

#define LOLA_LATENCY_SERVICE_NO_FULL_RESPONSE_RETRY_DURATION_MILLIS 100
#define LOLA_LATENCY_SERVICE_NO_REPLY_TIMEOUT_MILLIS (1000 + LATENCY_PING_DATA_POINT_STACK_SIZE*(LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS+LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS))
#define LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS 5000

#define PACKET_DEFINITION_PING_HEADER (PACKET_DEFINITION_CONNECTION_HEADER+1)
#define PACKET_DEFINITION_PING_PAYLOAD_SIZE 0

class LatencyLoLaService : public IPacketSendService
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
		WaitingForAck,
		BackOff,
		ShortTimeOut,
		LongTimeOut,
		AnalysingResults,
		Done
	} State = LatencyServiceStateEnum::Done;

	LoLaPacketNoPayload PacketHolder;//Optimized memory usage grunt packet.

	uint32_t LastStartedMillis = 0;
	uint32_t FirstStartedMillis = 0;
	uint32_t LastSentTimeStamp = 0;
	volatile uint8_t SentId;

	uint16_t ReceivingDuration = 0;
	uint32_t DurationSum = 0;
	uint8_t SampleCount = 0;

	//Callback handler
	Signal<const bool> MeasurementCompleteEvent;


public:
	LatencyLoLaService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LATENCY_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
	{
	}

	float GetLatency()
	{
		return ((float)GetRTT() / (float)2000);
	}

	uint32_t GetRTT()
	{
		return GetAverage();
	}

	void RequestRefreshPing()
	{
		Enable();
		State = LatencyServiceStateEnum::Setup;
	}

	void RequestSinglePing()
	{
		PreparePacket();
		RequestSendPacket(LOLA_LATENCY_SERVICE_NO_FULL_RESPONSE_RETRY_DURATION_MILLIS);
		State = LatencyServiceStateEnum::Done;
		Enable();
	}

	void SetMeasurementCompleteCallback(const Slot<const bool>& slot)
	{
		MeasurementCompleteEvent.attach(slot);
	}
	
private:
	uint16_t GetAverage()
	{
		if (SampleCount >= LATENCY_PING_DATA_POINT_STACK_SIZE)
		{
			return DurationSum / SampleCount;
		}
		else
		{
			return 0;
		}
	}

	void ClearDurations()
	{
		SampleCount = 0;
		DurationSum = 0;
	}

	void PreparePacket()
	{
		SentId = random(0xFF);
		PacketHolder.SetDefinition(&PingDefinition);
		PacketHolder.SetId(SentId);
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

	bool ShouldWakeUpOnOutsidePacket()
	{
		if (IsSetupOk() &&
			((State == LatencyServiceStateEnum::Done) || (State == LatencyServiceStateEnum::ShortTimeOut)))
		{
			SetNextRunDelayRandom(LOLA_LATENCY_SERVICE_NO_FULL_RESPONSE_RETRY_DURATION_MILLIS);
			return true;
		}

		return false;
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_PING_HEADER)
		{
			ShouldWakeUpOnOutsidePacket();
			return true;
		}
		return false;
	}

	bool ProcessAck(const uint8_t header, const uint8_t id)
	{
		ReceivingDuration = Micros() - LastSentTimeStamp;

		if (header == PACKET_DEFINITION_PING_HEADER)
		{
			if (!ShouldWakeUpOnOutsidePacket() && ShouldRespondToOutsidePackets())
			{
				if (State == LatencyServiceStateEnum::Sending || State == LatencyServiceStateEnum::WaitingForAck)
				{
					if (LastSentTimeStamp != 0 && SentId == id &&
						ReceivingDuration < LATENCY_SERVICE_PING_TIMEOUT_MICROS)
					{
						DurationSum += ReceivingDuration;
						SampleCount++;
					}
					State = LatencyServiceStateEnum::BackOff;
					SetNextRunASAP();
				}
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	bool OnSetup()
	{
		if (!IPacketSendService::OnSetup())
		{
			return false;
		}

		State = LatencyServiceStateEnum::Done;
		SetNextRunDefault();
		Disable();
		return true;
	}

	void OnSendOk()
	{
		SetNextRunASAP();
	}

	void OnSendDelayed()
	{
		LastSentTimeStamp = Micros();
#ifdef DEBUG_LOLA
		SendDelays++;
#endif		
	}
	void OnSendRetrying()
	{
		LastSentTimeStamp = Micros();
#ifdef DEBUG_LOLA
		SendFails++;
#endif
	}

	void OnSendFailed()
	{
#ifdef DEBUG_LOLA
		Serial.println(F("OnSendFailed"));
		DebugCurrentSend();
#endif
		SetNextRunDefault();
	}

	void OnSendTimedOut()
	{
#ifdef DEBUG_LOLA 
		Serial.println(F("OnSendTimedOut"));
		DebugCurrentSend();
#endif
	}

#ifdef DEBUG_LOLA
	void DebugCurrentSend()
	{

		if (SendDelays > 0)
		{
			Serial.print(F("Delays: "));
			Serial.println(SendDelays);
		}
		if (SendFails > 0)
		{
			Serial.print(F("Fails: "));
			Serial.println(SendFails);
		}
	}
#endif

#ifdef DEBUG_LOLA
	uint8_t SendDelays, SendFails;

#endif
	void OnService()
	{
		switch (State)
		{
		case LatencyServiceStateEnum::Setup:
#ifdef DEBUG_LOLA
			Serial.println(F("Latency service started"));
#endif			
			ClearDurations();
			FirstStartedMillis = Millis();
			State = LatencyServiceStateEnum::Starting;
			SetNextRunDefault();
			break;
		case LatencyServiceStateEnum::Starting:
#ifdef DEBUG_LOLA
			Serial.println(F("Measuring..."));
#endif	
			ClearDurations();
			LastSentTimeStamp = 0;
#ifdef DEBUG_LOLA
			SendDelays = 0;
			SendFails = 0;
#endif
			
#ifdef MOCK_RADIO
#ifdef DEBUG_LOLA
			Serial.println(F("Mock Latency done."));
#endif
			State = LatencyServiceStateEnum::Done;
			MeasurementCompleteEvent.fire(true);
#else
			LastStartedMillis = Millis();
			State = LatencyServiceStateEnum::Checking;
#endif
			SetNextRunASAP();

			break;
		case LatencyServiceStateEnum::Checking:
			//Have we timed out for good?
			if (Millis() - FirstStartedMillis > LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS)
			{
				//TODO: Handle retry
#ifdef DEBUG_LOLA
				Serial.println(F("Latency timed out for good."));
#endif
				//FirstStartedMillis = 0;//Reset the long time out counter.
				State = LatencyServiceStateEnum::Done;
#ifdef MOCK_RADIO
				MeasurementCompleteEvent.fire(true);
#else
				MeasurementCompleteEvent.fire(false);
#endif
				
				SetNextRunASAP();
			}
			//Have we timed out for a worst case scenario measurement?
			else if (Millis() - LastStartedMillis > LOLA_LATENCY_SERVICE_NO_REPLY_TIMEOUT_MILLIS)
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Latency timed out."));
#endif
				State = LatencyServiceStateEnum::ShortTimeOut;
				SetNextRunDelayRandom(LOLA_LATENCY_SERVICE_NO_FULL_RESPONSE_RETRY_DURATION_MILLIS);
			}
			//Do we have needed sample count?
			else if (SampleCount >= LATENCY_PING_DATA_POINT_STACK_SIZE)
			{
				State = LatencyServiceStateEnum::AnalysingResults;
				SetNextRunASAP();
			}
			//All right, lets send a packet and get a sample!
			else
			{
				State = LatencyServiceStateEnum::Sending;
				PreparePacket();
				RequestSendPacket((uint8_t)(LATENCY_SERVICE_PING_TIMEOUT_MICROS / 1000));
				LastSentTimeStamp = Micros();
			}
			break;
		case LatencyServiceStateEnum::Sending:
			State = LatencyServiceStateEnum::WaitingForAck;
			SetNextRunDelay(LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS);
			break;
		case LatencyServiceStateEnum::WaitingForAck:
			//If we're here, it means the ack failed to arrive.
			State = LatencyServiceStateEnum::Checking;
			SetNextRunASAP();
			break;
		case LatencyServiceStateEnum::BackOff:
			//If we're here, it means the ack arrived and was valid.
			LastSentTimeStamp = 0; //Make sure we ignore stale acks.
			State = LatencyServiceStateEnum::Checking;
			//Do we have needed sample count?
			if (SampleCount >= LATENCY_PING_DATA_POINT_STACK_SIZE)
			{
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelayRandom(LOLA_LATENCY_SERVICE_SEND_BACK_OFF_DURATION_MILLIS);
			}
			break;
		case LatencyServiceStateEnum::ShortTimeOut:
			State = LatencyServiceStateEnum::Starting;
			SetNextRunASAP();
			break;
		case LatencyServiceStateEnum::LongTimeOut:
			State = LatencyServiceStateEnum::Starting;
			FirstStartedMillis = Millis();
			SetNextRunASAP();
			break;
		case LatencyServiceStateEnum::AnalysingResults:
			if (GetAverage() != 0)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Latency measurement took: "));
				Serial.print(Millis() - LastStartedMillis);
				Serial.println(F(" ms"));

				DebugCurrentSend();
				Serial.print(F("RTT: "));
				Serial.print(GetRTT());
				Serial.println(F(" us"));
				Serial.print(F("Latency: "));
				Serial.print(GetLatency(), 2);
				Serial.println(F(" ms"));
#endif
				State = LatencyServiceStateEnum::Done;
				MeasurementCompleteEvent.fire(true);
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelay(LOLA_LATENCY_SERVICE_NO_FULL_RESPONSE_RETRY_DURATION_MILLIS);
				State = LatencyServiceStateEnum::Starting;
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