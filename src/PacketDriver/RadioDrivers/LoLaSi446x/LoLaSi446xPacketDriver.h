// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h


#include <SPI.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xRadioDriver.h>


class LoLaSi446xPacketDriver
	: public LoLaSi446xRadioDriver
{
private:

	uint32_t LastEventTimestamp = 0;

	uint8_t ReceivedPacketLength = 0;

	struct PendingActionsStruct
	{
		volatile bool RX = false;
		volatile bool Power = false;
		volatile bool Channel = false;

		void Clear()
		{
			RX = false;
			Power = false;
			Channel = false;
		}
	};

protected:
	PendingActionsStruct PendingActions;

public:
	LoLaSi446xPacketDriver(SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaSi446xRadioDriver(spi, cs, reset, irq)
	{
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		LoLaPacketDriver::Debug(serial);

		serial->print(F("Driver timeouts: "));
		serial->println(TimeoutCount);
	}
#endif

	//Return true when there is no more work to be done.
	bool CheckPending()
	{
		//Only update event timestamp if an interrupt hasn't touched it.
		if (LastEventTimestamp == 0 ||
			LastEventTimestamp == EventTimestamp)
		{
			EventTimestamp = micros();
		}

		LastEventTimestamp = EventTimestamp;
		if (CheckQueue(LastEventTimestamp))
		{
			if (PendingActions.RX)
			{
				SetRadioReceive();
			}
			else if (PendingActions.Power)
			{
				SetRadioPower();
			}
			else if (PendingActions.Channel)
			{
				SetRadioChannel();
			}
			else
			{
				return true;
			}
		}

		return false;
	}

protected:
	virtual void OnResetLiveData()
	{
		PendingActions.Clear();
	}

	virtual void OnChannelUpdated()
	{
		PendingActions.Channel = true;
		Wake();
	}

	virtual void OnTransmitPowerUpdated()
	{
		PendingActions.Power = true;
		Wake();
	}

	void SetRadioPower()
	{
		if (AllowedRadioChange() && SetRadioTransmitPower())
		{
			PendingActions.Power = false;
		}
		else
		{
			PendingActions.Power = true;
			Wake();
		}
	}

	void SetRadioChannel()
	{
		if (AllowedRadioChange())
		{
			SetRadioReceive();
			// Setting the channel and RX are the same in this radio, so we clear both flags.
			PendingActions.Channel = false;
			PendingActions.RX = false;
		}
		else
		{
			PendingActions.Channel = true;
			Wake();
		}
	}

	virtual void SetRadioReceive()
	{
		if (AllowedRadioChange() && SetRadioReceiveInternal())
		{
			OnRadioSetToRX();
			// Setting the channel and RX are the same in this radio, so we clear both flags.
			PendingActions.Channel = false;
			PendingActions.RX = false;
		}
		else
		{
			PendingActions.Channel = true;
			Wake();
		}
	}

	bool RadioSetup()
	{
#ifdef LOLA_MOCK_RADIO
		return true;
#else
		if (!LoLaSi446xRadioDriver::RadioSetup())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("LoLaSi446xRadioDriver setup failed."));
#endif
			return false;
		}

#ifdef DEBUG_RADIO_DRIVER
		Serial.print(F("Si4463 Present: "));
		//Serial.println(DeviceInfo.RevisionBranch);
		//Serial.println(DeviceInfo.RevisionExternal);
		//Serial.println(DeviceInfo.RevisionInternal);
		//Serial.println(DeviceInfo.ChipRevision);
		//Serial.println(DeviceInfo.Customer);
		Serial.println(DeviceInfo.PartId);
		//Serial.println(DeviceInfo.Patch);
		//Serial.println(DeviceInfo.PartBuild);
		//Serial.println(DeviceInfo.Function);
		//Serial.println(DeviceInfo.ROM);
#endif

		return true;
#endif
	}

private:
	// Return true if done.
	bool CheckQueue(const uint32_t timeMicros)
	{
		ReadInterruptFlags();

		// Valid PREAMBLE and SYNC detected.
		if (HasInterruptSyncDetect())
		{
			LastReceivedInfo.Micros = timeMicros;
			LastReceivedInfo.RSSI = GetLatchedRSSI();
			OnIncoming();
		}
		// Valid packet.
		else if (HasInterruptRX())
		{
			ReceivedPacketLength = GetReceivedLength();

			if (ReceivedPacketLength >= LOLA_PACKET_MIN_PACKET_SIZE &&
				ReceivedPacketLength < LOLA_PACKET_MAX_PACKET_SIZE)
			{
				if (AllowedReceive())
				{
					ReadReceived(ReceivedPacketLength);
					OnReceiveOk(ReceivedPacketLength);
				}
				else
				{
					// TODO: Retry again later?
					OnReceiveFail();
				}
			}
			else
			{
				OnReceiveFail();
			}
		}
		// Internal CRC is not used.
		// Corrupted packet.
		else if (HasInterruptCRCError())
		{
			OnReceiveFail();
		}
		// Packet sent.
		else if (HasInterruptPacketSent())
		{
			OnSentOk(timeMicros);
		}
		else if (HasInterruptLowBattery())
		{
			OnBatteryAlarm();
		}
		else if (HasInterruptWakeUpTimer())
		{
			return true;//Not used.
		}
		else
		{
			return true;
		}

		return false;
	}


};
#endif