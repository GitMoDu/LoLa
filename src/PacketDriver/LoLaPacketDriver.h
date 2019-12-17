// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLaDriver.h>
#include <Packet\LoLaPacketMap.h>
#include <PacketDriver\ILoLaSelector.h>

class LoLaPacketDriver : public ILoLaDriver
{
private:
	//Static helpers.
	uint32_t DuplexElapsed;
	uint8_t LastPower = 0;
	uint8_t LastChannel = 0;

	volatile DriverActiveStates DriverActiveState = DriverActiveStates::DriverDisabled;

	PacketDefinition* AckDefinition = nullptr;
	PacketDefinition* AckReverseDefinition = nullptr;

	class EncoderDurations
	{
	public:
		uint32_t ShortEncode = 0;
		uint32_t LongEncode = 0;

		uint32_t ShortDecode = 0;
		uint32_t LongDecode = 0;

		//uint32_t ShortTransmit = 0;
		//uint32_t LongTransmit = 0;


	};

#ifdef DEBUG_LOLA
	EncoderDurations EncoderTiming;
#endif

protected:

	//template<const uint8_t ExtendedMaxPacketSize>
	class NotifiableIncomingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
	{
	public:
		NotifiableIncomingPacket() : TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>()
		{

		}

		bool NotifyPacketReceived(const uint8_t id, uint8_t* payload, const uint32_t timestamp)
		{
			return this->GetDefinition()->Service->OnPacketReceived(this->GetDefinition(), id, payload, timestamp);
		}
	};

	class NotifiableOutgoingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
	{
	public:
		NotifiableOutgoingPacket() : TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>()
		{

		}

		bool NotifyPacketTransmitted(const uint8_t header,const uint8_t id,const uint32_t timestamp)
		{
			return this->GetDefinition()->Service->OnPacketTransmited(header, id, timestamp);
		}
	};

	NotifiableIncomingPacket IncomingPacket;

	NotifiableOutgoingPacket OutgoingPacket;

	TemplateLoLaPacket<LOLA_PACKET_MIN_PACKET_SIZE> AckPacket;

protected:
	//Radio Driver implementation.
	virtual bool RadioSetup() { return false; }
	virtual void SetRadioReceive() {}
	virtual void SetRadioChannel() {}
	virtual void SetRadioPower() {}
	virtual bool RadioTransmit() { return false; }

public:
	//Packet Driver implementation.
	virtual void OnChannelUpdated() {}
	virtual void OnTransmitPowerUpdated() {}

public:
	LoLaPacketDriver() : ILoLaDriver()
	{
	}

	bool Setup()
	{
		AckDefinition = PacketMap.GetDefinition(PACKET_DEFINITION_ACK_HEADER);
		if (SyncedClock != nullptr && AckDefinition != nullptr)
		{
			return RadioSetup() && MeasureRadioTimings();
		}

		return false;
	}

	virtual bool SendPacket(ILoLaPacket* transmitPacket)
	{
		if (DriverActiveState != DriverActiveStates::ReadyForAnything ||
			(transmitPacket->GetDefinition() != nullptr))
		{
			return false;
		}

		DriverActiveState = DriverActiveStates::SendingOutgoing;

		OutgoingPacket.SetDefinition(transmitPacket->GetDefinition());
		LastValidSentInfo.Header = OutgoingPacket.GetDataHeader();
		LastValidSentInfo.Id = OutgoingPacket.GetId();

		OutgoingPacket.SetMACCRC(CryptoEncoder.Encode(transmitPacket->GetContent(), OutgoingPacket.GetDefinition()->GetContentSize(), OutgoingPacket.GetContent()));

		return RadioTransmit();
	}

	virtual bool AllowedReceive()
	{
		if (DriverActiveState != DriverActiveStates::BlockedForIncoming)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bad state OnReceiveOk: "));
			Serial.println(DriverActiveState);
#endif
			return false;
		}

		return true;
	}


protected:
	///Driver calls.///

	//When RF detects incoming packet.
	void OnIncoming()
	{
		if (DriverActiveState == DriverActiveStates::ReadyForAnything)
		{
			DriverActiveState = DriverActiveStates::BlockedForIncoming;
		}
		else
		{
			if (DriverActiveState == DriverActiveStates::SendingOutgoing)
			{
				//Got packet right after sending one, let's get back to that received packet.
				DriverActiveState = DriverActiveStates::BlockedForIncoming;
			}
			else
			{
				StateCollisionCount++;
#ifdef DEBUG_LOLA
				Serial.print(F("Bad state OnIncoming: "));
				Serial.println(DriverActiveState);
#endif
			}
		}
	}

	//When RF has packet to read, driver calls this, after calling OnIncoming()
	bool OnReceiveOk(const uint8_t packetLength)
	{
		DriverActiveState = DriverActiveStates::ProcessingIncoming;

		if (CryptoEncoder.Decode(IncomingPacket.GetContent(), PacketDefinitionHelper::GetContentSizeQuick(packetLength), IncomingPacket.GetMACCRC()))
		{
			if (!IncomingPacket.SetDefinition(PacketMap.GetDefinition(IncomingPacket.GetDataHeader())))
			{
				// Packet not mapped, matching service does not exist.
#ifdef DEBUG_LOLA
				RejectedCount++;
#endif	
				return false;
			}

			// Packet received Ok, let's commit that info really quick.
			LastValidReceivedInfo.Micros = LastReceivedInfo.Micros;
			LastValidReceivedInfo.RSSI = LastReceivedInfo.RSSI;

			// Is it an Ack packet?
			if (IncomingPacket.GetDataHeader() == AckDefinition->Header)
			{
				// Get the reverse definition from the ack packet payload (Header).
				AckReverseDefinition = PacketMap.GetDefinition(IncomingPacket.GetPayload()[0]);

				// If definition exists, notify service.
				if (AckReverseDefinition != nullptr)
				{
					AckReverseDefinition->Service->OnAckReceived(IncomingPacket.GetPayload()[0], IncomingPacket.GetId(), LastValidReceivedInfo.Micros);
				}
			}
			else
			{
				if (IncomingPacket.NotifyPacketReceived(IncomingPacket.GetId(), IncomingPacket.GetPayload(), LastValidReceivedInfo.Micros))
				{
					// If service validation passed and packet has Ack property..
					if (IncomingPacket.GetDefinition()->HasACK())
					{
						// Send Ack ASAP.
						AckPacket.SetDefinition(AckDefinition);
						AckPacket.GetPayload()[0] = IncomingPacket.GetDefinition()->Header;
						AckPacket.SetId(IncomingPacket.GetId());
						DriverActiveState = DriverActiveStates::SendingOutgoing;
						if (SendPacket(&AckPacket))
						{
							return false; //Radio restores to RX after TX, don't restore it manually.
						}
#ifdef DEBUG_LOLA
						else
						{
							Serial.println(F("Send Ack failed."));
						}
#endif				
					}
				}
				ReceivedCount++;

				//Check for packet collisions.
				CheckReceiveCollision(micros() - LastValidReceivedInfo.Micros);
			}
		}
		else
		{
			//CRC mismatch, packet malformed or no intended for this device.
#ifdef DEBUG_LOLA
			RejectedCount++;
#endif	
			return false;
		}
		return true;
	}

	bool OnSentOk(const uint32_t timestamp)
	{
		if (DriverActiveState == DriverActiveStates::SendingOutgoing)
		{
			LastValidSentInfo.Micros = timestamp;
			TransmitedCount++;
			OutgoingPacket.NotifyPacketTransmitted(LastValidSentInfo.Header, LastValidSentInfo.Id, timestamp);
			OutgoingPacket.ClearDefinition();

			return true;
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bad state OnSentOk: "));
			Serial.println(DriverActiveState);
#endif
			return false;
		}
	}

	void OnBatteryAlarm()
	{
		//TODO: Store statistics.
	}

	//When RF has failed to receive packet.
	void OnReceiveFail()
	{
#ifdef DEBUG_LOLA
		Serial.print(F("OnReceiveFail: "));
		Serial.println(DriverActiveState);
#endif
	}

protected:
	bool AllowedRadioChange()
	{
		return DriverActiveState == DriverActiveStates::ReadyForAnything;
	}

public:
	bool AllowedSend()
	{
		if (DriverActiveState != DriverActiveStates::ReadyForAnything)
		{
			return false;
		}
		else if (HasLink())
		{
			return IsInSendSlot();
		}
		else
		{
			return GetElapsedMillisLastValidSent() >= BackOffPeriodUnlinkedMillis;
		}
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		ILoLaDriver::Debug(serial);
		serial->println("Radio timings");

		serial->print(" ETTM: ");
		serial->print(ETTM);
		serial->println(" us");

		serial->print(" ShortEncode: ");
		serial->print(EncoderTiming.ShortEncode);
		serial->println(" us");

		serial->print(" LongEncode: ");
		serial->print(EncoderTiming.LongEncode);
		serial->println(" us");

		serial->print(" ShortDecode: ");
		serial->print(EncoderTiming.ShortDecode);
		serial->println(" us");

		serial->print(" LongDecode: ");
		serial->print(EncoderTiming.LongDecode);
		serial->println(" us");

	}
#endif

private:
	void CheckReceiveCollision(const uint32_t offsetMicros)
	{
		if (HasLink() &&
			(offsetMicros > LOLA_LINK_COLLISION_SLOT_RANGE_MICROS) ||
			(!IsInReceiveSlot(offsetMicros)))
		{
			TimingCollisionCount++;
		}
	}

	bool IsInReceiveSlot(const int32_t offsetMicros)
	{
		DuplexElapsed = (SyncedClock->GetSyncMicros() + offsetMicros) % DuplexPeriodMicros;

		//Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed >= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if (DuplexElapsed <= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}

	bool IsInSendSlot()
	{
		DuplexElapsed = (SyncedClock->GetSyncMicros() + ETTM) % DuplexPeriodMicros;

		//Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed <= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if ((DuplexElapsed >= HalfDuplexPeriodMicros) &&
				DuplexElapsed <= DuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}


	// Assumes encoding and CRC takes the around same time
	bool MeasureRadioTimings()
	{
		ETTM = 0;

		const uint32_t TimeoutMillis = 10000;

		const uint32_t Timeout = micros() + TimeoutMillis * 1000;
		OutgoingPacket.ClearDefinition();
		uint32_t Measure = 0;
		uint32_t Sum = 0;

		// Measure CRC and encode for short packets.
		Measure = micros();
		CryptoEncoder.EmptyEncode(OutgoingPacket.GetContent(), LOLA_PACKET_MIN_PACKET_SIZE);
		Sum += micros() - Measure;
#ifdef DEBUG_LOLA
		EncoderTiming.ShortEncode = Sum;
#endif
		// Measure CRC and encode for long packets.
		Measure = micros();
		CryptoEncoder.EmptyEncode(OutgoingPacket.GetContent(), LOLA_PACKET_MAX_PACKET_SIZE);
		Sum += micros() - Measure;
#ifdef DEBUG_LOLA
		EncoderTiming.LongEncode = Sum - EncoderTiming.ShortEncode;
#endif

		// Measure CRC and decode for short packets.
		Measure = micros();
		CryptoEncoder.EmptyDecode(OutgoingPacket.GetContent(), LOLA_PACKET_MIN_PACKET_SIZE);
		Sum += micros() - Measure;
#ifdef DEBUG_LOLA
		EncoderTiming.ShortDecode = Sum - EncoderTiming.LongEncode;
#endif

		// Measure CRC and decode for long packets.
		Measure = micros();
		CryptoEncoder.EmptyEncode(OutgoingPacket.GetContent(), LOLA_PACKET_MAX_PACKET_SIZE);
		Sum += micros() - Measure;
#ifdef DEBUG_LOLA
		EncoderTiming.LongDecode = Sum - EncoderTiming.ShortDecode;
#endif

		ETTM += Sum	/ 4;


		//// Measure send for short packets.
		//Sum = 0;
		//PacketDefinition TestShortDefinition(nullptr, 0, LOLA_PACKET_MIN_PACKET_SIZE);

		//OutgoingPacket.SetDefinition(&TestShortDefinition);

		//for (uint8_t i = 0; i < RepeatCount; i++)
		//{
		//	if (AllowedSend(0))
		//	{
		//		Measure = micros();
		//		SendPacket(&OutgoingPacket);
		//		Sum = micros() - Measure;
		//	}
		//	else if (micros() > Timeout)
		//	{
		//		return false;
		//	}
		//}
		//EncoderTiming.ShortTransmit = Sum / RepeatCount;

		//// Measure send for long packets.
		//Sum = 0;
		//PacketDefinition TestLongDefinition(nullptr, 0, LOLA_PACKET_MAX_PACKET_SIZE);

		//OutgoingPacket.SetDefinition(&TestLongDefinition);

		//for (uint8_t i = 0; i < RepeatCount; i++)
		//{
		//	if (AllowedSend(0))
		//	{
		//		Measure = micros();
		//		SendPacket(&OutgoingPacket);
		//		Sum = micros() - Measure;
		//	}
		//	else if (micros() > Timeout)
		//	{
		//		return false;
		//	}
		//}
		//EncoderTiming.LongTransmit = Sum / RepeatCount;

		return true;
	}

};
#endif