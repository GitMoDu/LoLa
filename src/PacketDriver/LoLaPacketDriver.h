// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLaDriver.h>
#include <LoLaClock\TickTrainedTimerSyncedClock.h>



class LoLaPacketDriver : public ILoLaDriver
{
private:
	class NotifiableAckReverseDefinition
	{
	private:
		PacketDefinition* OriginalDefinition = nullptr;
		PacketDefinition* AckDefinition = nullptr;

	public:
		bool SetOriginalDefinition(PacketDefinition* originalDefinition)
		{
			// If definition exists and is not recursively pointing to Ack.
			if (originalDefinition != nullptr &&
				originalDefinition != AckDefinition)
			{
				OriginalDefinition = originalDefinition;

				return true;
			}
			else
			{
				return false;
			}
		}

		bool SetAckDefinition(PacketDefinition* ackDefinition)
		{
			AckDefinition = ackDefinition;

			return AckDefinition != nullptr;
		}

		void NotifyAckReceived(const uint8_t id, const uint32_t timestamp)
		{
			OriginalDefinition->Service->OnPacketTransmited(OriginalDefinition->Header, id, timestamp);
		}
	};

	class NotifiableIncomingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
	{
	public:
		bool NotifyPacketReceived(const uint32_t timestamp)
		{

			return GetDefinition()->Service->OnPacketReceived(GetDefinition()->Header, GetId(), timestamp, GetPayload());
		}
	};

	class NotifiableOutgoingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
	{
	public:
		void NotifyPacketTransmitted(const uint8_t id, const uint32_t timestamp)
		{
			TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>::GetDefinition()->Service->OnPacketTransmited(GetDefinition()->Header, id, timestamp);
		}
	};

	// Timings.
	const uint32_t DuplexPeriodMicros = ILOLA_DUPLEX_PERIOD_MILLIS * (uint32_t)1000;
	const uint32_t HalfDuplexPeriodMicros = DuplexPeriodMicros / 2;
	const uint32_t BackOffPeriodUnlinkedMillis = LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS;
	
	TickTrainedTimerSyncedClock MasterSyncClock;


	// Static helpers.
	uint32_t DuplexElapsed;

	volatile DriverActiveStates DriverActiveState = DriverActiveStates::DriverDisabled;

	PacketDefinition* AckDefinition = nullptr;
	NotifiableAckReverseDefinition AckNotifier;

protected:
	NotifiableIncomingPacket IncomingPacket;
	NotifiableOutgoingPacket OutgoingPacket;

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
	LoLaPacketDriver() : ILoLaDriver(), AckNotifier(), MasterSyncClock()
	{
		SetSyncedClock(&MasterSyncClock);
	}

	bool Setup()
	{
		if (!ILoLaDriver::Setup())
		{
			return false;
		}

		AckDefinition = PacketMap.GetDefinition(PACKET_DEFINITION_ACK_HEADER);

		if (SyncedClock != nullptr &&
			AckDefinition != nullptr &&
			MasterSyncClock.Setup() &&
			AckNotifier.SetAckDefinition(AckDefinition))
		{
			return RadioSetup();
		}

		return false;
	}

	virtual bool SendPrePopulatedAck()
	{
		DriverActiveState = DriverActiveStates::SendingOutgoing;

		OutgoingPacket.SetMACCRC(CryptoEncoder->Encode(OutgoingPacket.GetContent(), OutgoingPacket.GetDefinition()->GetContentSize(), OutgoingPacket.GetContent()));

		return RadioTransmit();
	}

	virtual bool SendPacket(ILoLaPacket* transmitPacket)
	{
		DriverActiveState = DriverActiveStates::SendingOutgoing;

		OutgoingPacket.SetDefinition(transmitPacket->GetDefinition());

		// Store unencrypted Id for later notification.
		LastValidSentInfo.Id = transmitPacket->GetId();

		OutgoingPacket.SetMACCRC(CryptoEncoder->Encode(transmitPacket->GetContent(), OutgoingPacket.GetDefinition()->GetContentSize(), OutgoingPacket.GetContent()));

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
	virtual void OnStart()
	{


	}


protected:
	///Driver calls.///

	// When radio has been set to RX.
	void OnRadioSetToRX()
	{
		DriverActiveState = DriverActiveStates::ReadyForAnything;
	}

	// When RF detects incoming packet.
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
				// Got packet right after sending one, let's get back to that received packet.
				DriverActiveState = DriverActiveStates::BlockedForIncoming;
#ifdef DEBUG_LOLA
				Serial.print(F("Got packet right after sending one, let's get back to that received packet. DriverState:: "));
				Serial.println(DriverActiveState);
#endif
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

	// When RF has packet to read, driver calls this, after calling OnIncoming()
	void OnReceiveOk(const uint8_t packetLength)
	{
		bool RestoreToRXOnReceive = true;

		if (!CryptoEncoder->Decode(IncomingPacket.GetContent(), PacketDefinitionHelper::GetContentSizeQuick(packetLength), IncomingPacket.GetMACCRC()))
		{
			// CRC mismatch, packet malformed or not intended for this device.
		}
		else if (!IncomingPacket.SetDefinition(PacketMap.GetDefinition(IncomingPacket.GetDataHeader())))
		{
			// Packet not mapped, matching service does not exist.
#ifdef DEBUG_LOLA
			RejectedCount++;
#endif	
		}
		else
		{
			// Is it an Ack packet?
			if (IncomingPacket.GetDefinition() == AckDefinition)
			{
				// Get the reverse definition from the ack packet payload (Header).
				if (AckNotifier.SetOriginalDefinition(PacketMap.GetDefinition(IncomingPacket.GetPayload()[0])))
				{
					// If accepted, notify service.
					AckNotifier.NotifyAckReceived(IncomingPacket.GetId(), LastReceivedInfo.Micros);
				}
			}
			else
			{
				// Packet received Ok, let's commit that info really quick.
				LastValidReceivedInfo.Micros = LastReceivedInfo.Micros;
				LastValidReceivedInfo.RSSI = LastReceivedInfo.RSSI;
				ReceivedCount++;

#ifdef DEBUG_LOLA
				// Check for packet collisions.
				CheckReceiveCollision(micros() - LastValidReceivedInfo.Micros);
#endif

				if (IncomingPacket.NotifyPacketReceived(LastValidReceivedInfo.Micros) &&
					IncomingPacket.GetDefinition()->HasACK())
				{
					// If service validation passed and packet has Ack property,
					// prefill OutgoingPacket and send Ack ASAP.
					OutgoingPacket.SetDefinition(AckDefinition);
					OutgoingPacket.GetPayload()[0] = IncomingPacket.GetDefinition()->Header;
					OutgoingPacket.SetId(IncomingPacket.GetId());
					if (SendPrePopulatedAck())
					{
						// Radio restores to RX after TX, don't restore it manually.
						RestoreToRXOnReceive = false;
					}
					else
					{
#ifdef DEBUG_LOLA
						Serial.println(F("Send Ack failed."));
#endif				
					}
				}
			}
		}

		if (RestoreToRXOnReceive)
		{
			SetRadioReceive();
		}
	}

	void OnSentOk(const uint32_t timestamp)
	{
		if (DriverActiveState != DriverActiveStates::SendingOutgoing)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bad state OnSentOk: "));
			Serial.println(DriverActiveState);
#endif
		}

		// When radio is set to receive, State will be restored.
		SetRadioReceive();

		// Acks don't get transmission notification.
		if (OutgoingPacket.GetDefinition() != AckDefinition)
		{
			LastValidSentInfo.Micros = timestamp;
			TransmitedCount++;
			OutgoingPacket.NotifyPacketTransmitted(LastValidSentInfo.Id, timestamp);
		}
	}

	void OnBatteryAlarm()
	{
		//TODO: Store statistics.
	}

	// When RF has failed to receive packet.
	void OnReceiveFail()
	{
#ifdef DEBUG_LOLA
		Serial.print(F("OnReceiveFail: "));
		Serial.println(DriverActiveState);
#endif
		SetRadioReceive();
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
	}
#endif

private:
	bool IsInSendSlot()
	{
		DuplexElapsed = (SyncedClock->GetSyncMicros() + ETTM) % DuplexPeriodMicros;

		// Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed < HalfDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if (DuplexElapsed >= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}

#ifdef DEBUG_LOLA
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
#endif

};
#endif