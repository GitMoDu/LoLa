// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLaDriver.h>
#include <Packet\LoLaPacketMap.h>

#include <Services\LoLaServicesManager.h>
#include <PacketDriver\AsyncActionCallback.h>


class LoLaPacketDriver : public ILoLaDriver
{
private:
	///Async Callback.
	enum DriverAsyncActions : uint8_t
	{
		ActionProcessIncomingPacket = 0,
		ActionProcessSentOk = 1,
		ActionUpdatePower = 2,
		ActionUpdateChannel = 3,
		ActionAsyncRestore = 4,
		ActionProcessBatteryAlarm = 0xff
	};
	class ActionCallbackClass
	{
	public:
		uint8_t Action;
		uint8_t Value;

	} ActionGrunt;

	//Event queue, very rarely goes over 2.
	TemplateAsyncActionCallback<4, ActionCallbackClass> CallbackHandler;
	///

	//Async helpers for values.
	volatile uint8_t LastSentHeader = 0xFF;
	uint8_t OutgoingHeaderHelper = 0;

	uint8_t LastPower = 0;
	uint8_t LastChannel = 0;
	bool ChannelPending = false;

	volatile DriverActiveStates DriverActiveState = DriverActiveStates::DriverDisabled;

	PacketDefinition* AckDefinition = nullptr;

protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///

	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> IncomingPacket;
	uint8_t IncomingPacketSize = 0;

	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> OutgoingPacket;
	uint8_t OutgoingPacketSize = 0;

	TemplateLoLaPacket<LOLA_PACKET_MIN_PACKET_SIZE> AckPacket;


protected:
	//Driver implementation.
	virtual bool SetupRadio() { return false; }
	virtual void ReadReceived() {}
	virtual void SetToReceiving() {}
	virtual void SetRadioPower() {}
	virtual bool Transmit() { return false; }
	virtual bool CanTransmit() { return true; }
	virtual void DisableInterrupts() {}
	virtual void EnableInterrupts() {}

public:
	LoLaPacketDriver(Scheduler* scheduler) : ILoLaDriver(), Services(), CallbackHandler(scheduler)
	{
	}

	LoLaServicesManager* GetServices()
	{
		return &Services;
	}

private:
	void AddAsyncAction(const uint8_t action, const uint8_t value, const bool easeRetry = false)
	{
		ActionGrunt.Action = action;
		ActionGrunt.Value = value;

		CallbackHandler.AppendToQueue(ActionGrunt, easeRetry);
	}

	void OnAsyncAction(ActionCallbackClass action)
	{
		switch (action.Action)
		{
		case DriverAsyncActions::ActionProcessIncomingPacket:
			ProcessIncoming();
			break;
		case DriverAsyncActions::ActionProcessSentOk:
			ProcessSent(action.Value);
			break;
		case DriverAsyncActions::ActionProcessBatteryAlarm:
#ifdef DEBUG_LOLA
			Serial.println(F("ActionProcessBatteryAlarm"));
#endif
			break;
		case DriverAsyncActions::ActionUpdatePower:
			OnTransmitPowerUpdated();
			break;
		case DriverAsyncActions::ActionUpdateChannel:
			OnChannelUpdated();
			break;
		case DriverAsyncActions::ActionAsyncRestore:
			OnAsyncRestore();
			break;
		default:
			break;
		}
	}

	inline void OnTransmitted(const uint8_t header)
	{
		LastSentInfo.Micros = micros();
		LastSentHeader = header;
		LastChannel = CurrentChannel;
		OnTransmitPowerUpdated();
		//TODO: Add time out check. Maybe an action just for post transmit.
	}

protected:
	void OnStart()
	{
		LastChannel = 0xFF;
		LastPower = 0;
		ChannelPending = false;
		RestoreToReceiving();
	}

	void OnChannelUpdated()
	{
		if (LastChannel != CurrentChannel)
		{
			if (DriverActiveState == DriverActiveStates::ReadyForAnything)
			{
				RestoreToReceiving();
			}
			else
			{
				ChannelPending = true;
				AddAsyncAction(DriverAsyncActions::ActionUpdateChannel, 0, true);
			}
		}
	}

	void OnTransmitPowerUpdated()
	{
		if (LastPower != CurrentTransmitPower)
		{
			if (DriverActiveState == DriverActiveStates::ReadyForAnything)
			{
				SetRadioPower();
				LastPower = CurrentTransmitPower;
				RestoreToReceiving();
			}
			else
			{
				AddAsyncAction(DriverAsyncActions::ActionUpdatePower, 0, true);
			}
		}
	}

	void OnAsyncRestore()
	{
#ifdef DEBUG_LOLA
		Serial.print(F("RestoreToReceiving: "));
		Serial.println(DriverActiveState);
#endif
		LastSentHeader = 0xFF;
		RestoreToReceiving();
	}

public:
	///Public methods.
	bool Setup()
	{
		AckDefinition = PacketMap.GetDefinition(PACKET_DEFINITION_ACK_HEADER);
		if (AckDefinition != nullptr && SetupRadio())
		{
			MethodSlot<LoLaPacketDriver, ActionCallbackClass> memFunSlot(this, &LoLaPacketDriver::OnAsyncAction);
			CallbackHandler.AttachActionCallback(memFunSlot);

			return true;
		}

		return false;
	}

	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		if (ChannelPending ||
			(DriverActiveState != DriverActiveStates::ReadyForAnything &&
				DriverActiveState != DriverActiveStates::SendingAck))
		{
			return false;
		}

		DriverActiveState = DriverActiveStates::SendingOutgoing;

		OutgoingHeaderHelper = transmitPacket->GetDataHeader();

		OutgoingPacket.SetMACCRC(CryptoEncoder.Encode(transmitPacket->GetRawContent(), transmitPacket->GetDefinition()->GetContentSize(), OutgoingPacket.GetRawContent()));
		OutgoingPacketSize = transmitPacket->GetDefinition()->GetTotalSize();

		if (OutgoingPacketSize > 0 && Transmit())
		{
			OnTransmitted(OutgoingHeaderHelper);
			DriverActiveState = DriverActiveStates::WaitingForTransmissionEnd;

			return true;
		}
		else
		{
			DriverActiveState = DriverActiveStates::ReadyForAnything;
		}

		return false;
	}

private:
	void ProcessIncoming()
	{
		if (DriverActiveState != DriverActiveStates::AwaitingProcessing)
		{
			RestoreToReceiving();

			return;
		}

		DriverActiveState = DriverActiveStates::ProcessingIncoming;

		ReadReceived();

		if (IncomingPacketSize < LOLA_PACKET_MIN_PACKET_SIZE ||
			IncomingPacketSize > LOLA_PACKET_MAX_PACKET_SIZE)
		{
			RestoreToReceiving();

			return;//Invalid packet size;
		}

		if (CryptoEncoder.Decode(IncomingPacket.GetRawContent(), PacketDefinition::GetContentSizeQuick(IncomingPacketSize), IncomingPacket.GetMACCRC()) &&
			IncomingPacket.SetDefinition(PacketMap.GetDefinition(IncomingPacket.GetDataHeader())))
		{
			//Packet received Ok, let's commit that info really quick.
			LastValidReceivedInfo.Micros = LastReceivedInfo.Micros;
			LastValidReceivedInfo.RSSI = LastReceivedInfo.RSSI;
			ReceivedCount++;

			//Check for packet collisions.
			if (LinkActive && !IsInReceiveSlot(LastValidReceivedInfo.Micros))
			{
				TimingCollisionCount++;
			}


			//Is Ack packet.
			if (IncomingPacket.GetDefinition()->IsAck())
			{
				Services.ProcessAck(&IncomingPacket);
				RestoreToReceiving();
			}
			else if (IncomingPacket.GetDefinition()->HasACK())//If packet has ack, do service validation before sending Ack.
			{
				if (Services.ProcessAckedPacket(&IncomingPacket))
				{
					//Send Ack ASAP.
					AckPacket.SetDefinition(AckDefinition);
					AckPacket.GetPayload()[0] = IncomingPacket.GetDefinition()->GetHeader();
					AckPacket.SetId(IncomingPacket.GetId());
					DriverActiveState = DriverActiveStates::SendingAck;
					if (!SendPacket(&AckPacket))
					{
#ifdef DEBUG_LOLA
						Serial.println(F("Send Ack failed."));
#endif						
						RestoreToReceiving();
					}
				}
				else
				{
					//NACK.
					RestoreToReceiving();
				}
			}
			else
			{
				//Process packet directly, no Ack.
				Services.ProcessPacket(&IncomingPacket);
				RestoreToReceiving();
			}
		}
		else
		{
			//Failed to read incoming packet.
			RejectedCount++;
			RestoreToReceiving();
		}
	}

	void ProcessSent(const uint8_t header)
	{
		Services.ProcessSent(header);
		RestoreToReceiving();
	}

	void RestoreToReceiving()
	{
		LastChannel = CurrentChannel;
		ChannelPending = false;
		DriverActiveState = DriverActiveStates::ReadyForAnything;
		SetToReceiving();
	}

public:
	///Driver calls.
	//When RF detects incoming packet.
	void OnIncoming(const int16_t rssi)
	{
		LastReceivedInfo.Micros = micros();
		LastReceivedInfo.RSSI = rssi;

		if (DriverActiveState == DriverActiveStates::ReadyForAnything)
		{
			DriverActiveState = DriverActiveStates::BlockedForIncoming;
		}
#ifdef DEBUG_LOLA
		else
		{
			Serial.print(F("Badstate OnIncoming: "));
			Serial.println(DriverActiveState);
		}
#endif
	}

	//When RF has packet to read, copy content into receive buffer.
	void OnReceiveBegin(const uint8_t length, const int16_t rssi)
	{
		//DisableInterrupts();	//Not working properly.

		if (DriverActiveState == DriverActiveStates::BlockedForIncoming)
		{
			DriverActiveState = DriverActiveStates::AwaitingProcessing;
			IncomingPacketSize = length;
			LastReceivedInfo.RSSI = min(rssi, LastReceivedInfo.RSSI);

			AddAsyncAction(DriverAsyncActions::ActionProcessIncomingPacket, 0);
		}
		else
		{
			AddAsyncAction(DriverAsyncActions::ActionAsyncRestore, 0);
#ifdef DEBUG_LOLA
			Serial.print(F("Badstate OnReceiveBegin: "));
			Serial.println(DriverActiveState);
#endif
		}
	}

	//When RF has received a garbled packet.
	void OnReceivedFail(const int16_t rssi)
	{
#ifdef DEBUG_LOLA
		if (DriverActiveState != DriverActiveStates::BlockedForIncoming)
		{
			Serial.print(F("Bad state OnReceivedFail: "));
			Serial.println(DriverActiveState);
		}
#endif
		AddAsyncAction(DriverAsyncActions::ActionAsyncRestore, 0);
	}

	void OnSentOk()
	{
#ifdef DEBUG_LOLA
		if (DriverActiveState != DriverActiveStates::WaitingForTransmissionEnd)
		{
			Serial.print(F("Bad state OnSentOk: "));
			Serial.println(DriverActiveState);
		}
#endif

		LastValidSentInfo.Micros = LastSentInfo.Micros;
		TransmitedCount++;
		AddAsyncAction(DriverAsyncActions::ActionProcessSentOk, LastSentHeader);
		LastSentHeader = 0xFF;
	}

	void OnBatteryAlarm()
	{
		//TODO: Store statistics, maybe set-up a callback?
		AddAsyncAction(DriverAsyncActions::ActionProcessBatteryAlarm, 0);
	}

	void OnWakeUpTimer()
	{
		//TODO: Can this be used as stable clock source?
	}

	bool AllowedSend()
	{
		if (DriverActiveState != DriverActiveStates::ReadyForAnything ||
			ChannelPending)
		{
			return false;
		}

		if (LinkActive)
		{
			return GetElapsedMillisLastValidSent() >= BackOffPeriodLinkedMillis &&
				IsInSendSlot();
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
		Services.Debug(serial);
	}
#endif

private:
	bool IsInReceiveSlot(const uint32_t receivedMicros)
	{
		DuplexElapsed = SyncedClock.GetSyncMicros(receivedMicros) % DuplexPeriodMicros;

		//Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed >= HaldDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if (DuplexElapsed <= HaldDuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}

	bool IsInSendSlot()
	{
		DuplexElapsed = SyncedClock.GetSyncMicros() % DuplexPeriodMicros;

		//Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed < (HaldDuplexPeriodMicros - ETTM))
			{
				return true;
			}
		}
		else
		{
			if ((DuplexElapsed >= HaldDuplexPeriodMicros) &&
				DuplexElapsed < (DuplexPeriodMicros - ETTM))
			{
				return true;
			}
		}

		return false;
	}

};
#endif