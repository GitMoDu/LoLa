// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLa.h>
#include <Packet\LoLaPacketMap.h>

#include <Services\LoLaServicesManager.h>
#include <PacketDriver\AsyncActionCallback.h>


class LoLaPacketDriver : public ILoLa
{
private:
	enum DriverAsyncActions : uint8_t
	{
		ActionProcessIncomingPacket = 0,
		ActionProcessSentOk = 1,
		ActionUpdatePower = 2,
		ActionUpdateChannel = 3,
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

	//Async helpers for values.
	uint8_t LastPower = 0;
	uint8_t LastChannel = 0;

	volatile DriverActiveStates DriverActiveState = DriverActiveStates::DriverDisabled;
	volatile uint8_t LastSentHeader = 0xFF;

protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///

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
	LoLaPacketDriver(Scheduler* scheduler) : ILoLa(), Services(), CallbackHandler(scheduler)
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
		default:
			break;
		}
	}

	inline void OnTransmitted(const uint8_t header)
	{
		LastSentInfo.Time = millis();
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

public:
	///Public methods.
	bool Setup()
	{
		if (Receiver.Setup(&PacketMap, &CryptoEncoder) &&
			Sender.Setup(&PacketMap, &CryptoEncoder))
		{
			MethodSlot<LoLaPacketDriver, ActionCallbackClass> memFunSlot(this, &LoLaPacketDriver::OnAsyncAction);
			CallbackHandler.AttachActionCallback(memFunSlot);

			return SetupRadio();
		}

		return false;
	}

	bool SendPacket(ILoLaPacket* packet)
	{
		if (DriverActiveState != DriverActiveStates::ReadyForAnything)
		{
			RestoreToReceiving();
			return false;
		}

		DriverActiveState = DriverActiveStates::SendingOutgoing;
		if (Sender.SendPacket(packet) && Transmit())
		{
			OnTransmitted(packet->GetDataHeader());
			DriverActiveState = DriverActiveStates::WaitingForTransmissionEnd;

			return true;
		}

		RestoreToReceiving();

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

		if (!Receiver.ReceivePacket())
		{
			RejectedCount++;
			RestoreToReceiving();

			return;
		}

		//Packet received Ok, let's commit that info really quick.
		LastValidReceivedInfo.Time = LastReceivedInfo.Time;
		LastValidReceivedInfo.RSSI = LastReceivedInfo.RSSI;
		ReceivedCount++;

		//Is Ack packet.
		if (Receiver.GetIncomingDefinition()->IsAck())
		{
			Services.ProcessAck(Receiver.GetIncomingPacket());
			RestoreToReceiving();
		}
		else if (Receiver.GetIncomingDefinition()->HasACK())//If packet has ack, do service validation before sending Ack.
		{
			if (Services.ProcessAckedPacket(Receiver.GetIncomingPacket()))
			{
				CryptoEncoder.ResetCypherBlock();
				if (Sender.SendAck(Receiver.GetIncomingDefinition()->GetHeader(), Receiver.GetIncomingPacket()->GetId()) &&
					Transmit())
				{
					OnTransmitted(PACKET_DEFINITION_ACK_HEADER);
					DriverActiveState = DriverActiveStates::WaitingForTransmissionEnd;
				}
				else
				{
					//Failed to send.
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
			Services.ProcessPacket(Receiver.GetIncomingPacket());
			RestoreToReceiving();
		}

		Receiver.SetBufferSize(0);
	}

	void ProcessSent(const uint8_t header)
	{
		Services.ProcessSent(header);
		RestoreToReceiving();
	}

	void RestoreToReceiving()
	{
		CryptoEncoder.ResetCypherBlock();
		LastChannel = CurrentChannel;
		DriverActiveState = DriverActiveStates::ReadyForAnything;
		EnableInterrupts();
		SetToReceiving();
	}

public:
	///Driver calls.
	//When RF detects incoming packet.
	void OnIncoming(const int16_t rssi)
	{
		LastReceivedInfo.Time = millis();
		LastReceivedInfo.RSSI = rssi;

		if (DriverActiveState != DriverActiveStates::ReadyForAnything)
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Bad state OnIncoming"));
#endif
			RestoreToReceiving();
		}

		DriverActiveState = DriverActiveStates::BlockedForIncoming;
	}

	//When RF has packet to read, copy content into receive buffer.
	void OnReceiveBegin(const uint8_t length, const int16_t rssi)
	{
		DisableInterrupts();

		if (DriverActiveState == DriverActiveStates::BlockedForIncoming)
		{
			DriverActiveState = DriverActiveStates::AwaitingProcessing;
			Receiver.SetBufferSize(length);

			AddAsyncAction(DriverAsyncActions::ActionProcessIncomingPacket, 0);
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Badstate OnReceiveBegin"));
#endif
			RestoreToReceiving();
		}
	}

	//When RF has received a garbled packet.
	void OnReceivedFail(const int16_t rssi)
	{
#ifdef DEBUG_LOLA
		if (DriverActiveState != DriverActiveStates::BlockedForIncoming)
		{
			Serial.println(F("Bad state OnReceivedFail"));
		}
#endif
		RestoreToReceiving();
	}

	void OnSentOk()
	{
#ifdef DEBUG_LOLA
		if (DriverActiveState != DriverActiveStates::WaitingForTransmissionEnd)
		{
			Serial.println(F("Bad state OnSentOk"));
		}
#endif

		LastValidSentInfo.Time= LastSentInfo.Time;
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

	bool AllowedSend(const bool override = false)
	{
		if (LinkActive)
		{
			return DriverActiveState == DriverActiveStates::ReadyForAnything &&
				(override || IsInSendSlot());
		}
		else
		{
			return DriverActiveState == DriverActiveStates::ReadyForAnything &&
				override &&
				CoolAfterSend();
		}
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		ILoLa::Debug(serial);
		Services.Debug(serial);
	}
#endif

private:
	inline bool CoolAfterSend()
	{
		return LastSentInfo.Time == ILOLA_INVALID_MILLIS || millis() - LastSentInfo.Time > LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS;
	}

	bool IsInSendSlot()
	{
#ifdef USE_TIME_SLOT
		SendSlotElapsed = (GetSyncMillis() - (uint32_t)ETTM) % DuplexPeriodMillis;

		//Even spread of true and false across the DuplexPeriod
		if (EvenSlot)
		{
			if ((SendSlotElapsed < (DuplexPeriodMillis / 2)) &&
				SendSlotElapsed > 0)
			{
				return true;
			}
		}
		else
		{
			if ((SendSlotElapsed > (DuplexPeriodMillis / 2)) &&
				SendSlotElapsed < DuplexPeriodMillis)
			{
				return true;
			}
		}

		return false;
#else
		return true;
#endif
	}

};
#endif