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
	class IncomingInfoStruct
	{
	private:
		uint32_t PacketTime = ILOLA_INVALID_MILLIS;
		int16_t PacketRSSI = ILOLA_INVALID_RSSI;

	public:
		IncomingInfoStruct() {}

		uint32_t GetPacketTime()
		{
			return PacketTime;
		}

		int16_t GetPacketRSSI()
		{
			return PacketRSSI;
		}

		void Clear()
		{
			PacketTime = ILOLA_INVALID_MILLIS;
			PacketRSSI = ILOLA_INVALID_RSSI;
		}

		bool HasInfo()
		{
			return PacketTime != ILOLA_INVALID_MILLIS && PacketRSSI != ILOLA_INVALID_RSSI;
		}

		void SetInfo(const uint32_t time, const int16_t rssi)
		{
			PacketTime = time;
			PacketRSSI = rssi;
		}
	} IncomingInfo;

protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///

private:

	class ActionCallbackClass
	{
	public:
		uint8_t Action;
		uint8_t Value;

	} ActionGrunt;

	TemplateAsyncActionCallback<10, ActionCallbackClass> CallbackHandler;

	//Async helpers for values.
	uint8_t LastPower = 0;
	uint8_t LastChannel = 0;

public:
	LoLaPacketDriver(Scheduler* scheduler) : ILoLa(), Services(), CallbackHandler(scheduler)
	{

	}

	LoLaServicesManager* GetServices()
	{
		return &Services;
	}

	enum DriverActiveStates :uint8_t
	{
		ReadyForAnything,
		BlockedForIncoming,
		AwaitingProcessing,
		ProcessingIncoming,
		SendingOutgoing,
		SendingAck,
		WaitingForTransmissionEnd
	};

	enum DriverAsyncActions : uint8_t
	{
		ActionProcessIncomingPacket = 0,
		ActionProcessSentOk = 1,
		ActionUpdatePower = 2,
		ActionUpdateChannel = 3,
		ActionProcessBatteryAlarm = 0xff
	};

	volatile DriverActiveStates DriverActiveState = DriverActiveStates::ReadyForAnything;
	volatile uint8_t LastSentHeader = 0xFF;

private:
	inline void AddAsyncAction(const uint8_t action, const uint8_t value)
	{
		ActionGrunt.Action = action;
		ActionGrunt.Value = value;
		CallbackHandler.AppendToQueue(ActionGrunt);
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
			Serial.println(F("ActionProcessBatteryAlarm"));
			break;
		default:
			break;
		}
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
				SetToReceiving();
				LastChannel = CurrentChannel;
			}
			else
			{
				AddAsyncAction(DriverAsyncActions::ActionUpdateChannel, LastSentHeader);
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
			}
			else
			{
				AddAsyncAction(DriverAsyncActions::ActionUpdatePower, LastSentHeader);
			}
		}
	}

public:
	bool Setup()
	{
		if (Receiver.Setup(&PacketMap, &CryptoEncoder, &CryptoEnabled) &&
			Sender.Setup(&PacketMap, &CryptoEncoder, &CryptoEnabled))
		{
			IncomingInfo.Clear();

			MethodSlot<LoLaPacketDriver, ActionCallbackClass> memFunSlot(this, &LoLaPacketDriver::OnAsyncAction);
			CallbackHandler.AttachActionCallback(memFunSlot);

			return SetupRadio();
		}

		return false;
	}

public:
	///Public methods.
	bool SendPacket(ILoLaPacket* packet)
	{
		if (DriverActiveState != DriverActiveStates::ReadyForAnything)
		{
			Serial.println(F("Bad state SendPacket"));
		}

		DriverActiveState = DriverActiveStates::SendingOutgoing;
		if (Sender.SendPacket(packet) && Transmit())
		{
			LastSent = millis();
			LastSentHeader = packet->GetDataHeader();
			LastChannel = CurrentChannel;
			DriverActiveState = DriverActiveStates::WaitingForTransmissionEnd;

			return true;
		}

		RestoreToReceiving();

		return false;
	}

private:
	inline void ProcessIncoming()
	{
		if (DriverActiveState != DriverActiveStates::AwaitingProcessing)
		{
			Serial.println(F("Bad state ProcessIncoming"));
		}

		DriverActiveState = DriverActiveStates::ProcessingIncoming;
		if (!Enabled || !Receiver.ReceivePacket())
		{
			RejectedCount++;
			RestoreToReceiving();

			return;
		}

		//Packet received Ok, let's commit that info really quick.
		LastValidReceived = IncomingInfo.GetPacketTime();
		LastValidReceivedRssi = IncomingInfo.GetPacketRSSI();
		IncomingInfo.Clear();
		ReceivedCount++;

		//Is Ack packet.
		if (Receiver.GetIncomingDefinition()->IsAck())
		{
			Services.ProcessAck(Receiver.GetIncomingPacket());
			RestoreToReceiving();
		}
		else if (Receiver.GetIncomingDefinition()->HasACK())//If packet has ack, do service validation before sending Ack.
		{
			if (Services.ProcessAckedPacket(Receiver.GetIncomingPacket()) &&
				Sender.SendAck(Receiver.GetIncomingDefinition()->GetHeader(), Receiver.GetIncomingPacket()->GetId()) &&
				Transmit())
			{
				LastSent = millis();
				LastSentHeader = PACKET_DEFINITION_ACK_HEADER;
				LastChannel = CurrentChannel;
				DriverActiveState = DriverActiveStates::WaitingForTransmissionEnd;
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

	inline void ProcessSent(const uint8_t header)
	{
		if (DriverActiveState != DriverActiveStates::WaitingForTransmissionEnd)
		{
			Serial.println(F("Bad state OnIncoming"));
		}

		Services.ProcessSent(header);
		RestoreToReceiving();
	}

	void RestoreToReceiving()
	{
		DriverActiveState = DriverActiveStates::ReadyForAnything;
		SetToReceiving();
		EnableInterrupts();		
	}

public:
	///Driver calls.
	//When RF detects incoming packet.
	void OnIncoming(const int16_t rssi)
	{
		LastReceived = millis();
		LastReceivedRssi = rssi;

		if (DriverActiveState != DriverActiveStates::ReadyForAnything)
		{
			Serial.println(F("Bad state OnIncoming"));
		}

		DriverActiveState = DriverActiveStates::BlockedForIncoming;

		IncomingInfo.SetInfo(LastReceived, LastReceivedRssi);
	}

	//When RF has packet to read, copy content into receive buffer.
	void OnReceiveBegin(const uint8_t length, const int16_t rssi)
	{
		DisableInterrupts();

		Receiver.SetBufferSize(length);

		if (DriverActiveState != DriverActiveStates::BlockedForIncoming)
		{
			Serial.println(F("Badstate OnReceiveBegin"));
		}

		DriverActiveState = DriverActiveStates::AwaitingProcessing;
		ReadReceived();

		AddAsyncAction(DriverAsyncActions::ActionProcessIncomingPacket, 0);
	}

	//When RF has received a garbled packet.
	void OnReceivedFail(const int16_t rssi)
	{
		IncomingInfo.Clear();
		if (DriverActiveState != DriverActiveStates::BlockedForIncoming)
		{
			Serial.println(F("Bad state OnReceivedFail"));
		}
		SetToReceiving();
	}

	void OnSentOk()
	{
		LastValidSent = LastSent;

		if (DriverActiveState != DriverActiveStates::WaitingForTransmissionEnd)
		{
			Serial.println(F("Bad state OnSentOk"));
		}

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

	bool AllowedSend(const bool overridePermission = false)
	{
#ifdef USE_TIME_SLOT
		if (LinkActive)
		{
			return Enabled &&
				DriverActiveState == DriverActiveStates::ReadyForAnything &&
				(overridePermission || IsInSendSlot());
		}
		else
		{
			return Enabled &&
				overridePermission &&
				DriverActiveState == DriverActiveStates::ReadyForAnything;
		}
#else
		return Enabled &&
			!HotAfterSend() && !HotAfterReceive() &&
			DriverActiveState == DriverActiveStates::ReadyForAnything &&
			(overridePermission || LinkActive);
#endif
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		ILoLa::Debug(serial);
		Services.Debug(serial);
	}
#endif

protected:
	//Driver implementation.
	virtual bool SetupRadio() { return false; }
	virtual void ReadReceived() {}
	virtual void SetToReceiving() {}
	virtual void SetRadioPower() {}
	virtual bool Transmit() { return false; }
	virtual bool CanTransmit() { return true; }
	void DisableInterrupts() {}
	void EnableInterrupts() {}

private:
	inline bool HotAfterSend()
	{
		if (LastSent != ILOLA_INVALID_MILLIS && LastValidSent != ILOLA_INVALID_MILLIS)
		{
			if (LastSent - LastValidSent > 0)
			{
				return millis() - LastSent <= LOLA_LINK_SEND_BACK_OFF_DURATION_MILLIS;
			}
			else
			{
				return millis() - LastValidSent <= LOLA_LINK_RE_SEND_BACK_OFF_DURATION_MILLIS;
			}
		}
		else if (LastValidSent != ILOLA_INVALID_MILLIS)
		{
			return millis() - LastValidSent <= LOLA_LINK_RE_SEND_BACK_OFF_DURATION_MILLIS;
		}
		else if (LastSent != ILOLA_INVALID_MILLIS)
		{
			return millis() - LastSent <= LOLA_LINK_SEND_BACK_OFF_DURATION_MILLIS;
		}

		return false;
	}

	inline bool HotAfterReceive()
	{
		return millis() - LastReceived <= LOLA_LINK_RECEIVE_BACK_OFF_DURATION_MILLIS || LastReceived != ILOLA_INVALID_MILLIS;
	}

	inline bool IsInSendSlot()
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