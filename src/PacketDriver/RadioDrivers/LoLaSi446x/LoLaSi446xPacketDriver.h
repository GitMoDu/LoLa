// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h


#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xRadioDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\PendingActionsTracker.h>


class LoLaSi446xPacketDriver
	: public LoLaSi446xRadioDriver
{
private:

	uint32_t LastEventTimestamp = 0;

	PendingActionsTracker PendingActions;

public:
	LoLaSi446xPacketDriver(SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaSi446xRadioDriver(spi, cs, reset, irq)
		, PendingActions()
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
			if (PendingActions.GetRX())
			{
				SetRadioReceive();
			}
			else if (PendingActions.GetPower())
			{
				SetRadioPower();
			}
			else if (PendingActions.GetChannel())
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
		PendingActions.SetChannel();
		Wake();
	}

	virtual void OnTransmitPowerUpdated()
	{
		PendingActions.SetPower();
		Wake();
	}

	void SetRadioPower()
	{
		if (AllowedRadioChange() && SetRadioTransmitPower())
		{
			PendingActions.ClearPower();
		}
		else
		{
			PendingActions.SetPower();
			Wake();
		}
	}

	void SetRadioChannel()
	{
		if (AllowedRadioChange())
		{
			SetRadioReceive();
			PendingActions.ClearChannel();
		}
		else
		{
			PendingActions.SetChannel();
			Wake();
		}
	}

	bool RadioTransmit()
	{
#ifdef LOLA_MOCK_RADIO
		delayMicroseconds(500);
		return true;
#else

		if (!AllowedRadioChange())
		{
			return false;
		}

		return RadioTransmitInternal();
#endif
	}

	bool RadioSetup()
	{
#ifdef LOLA_MOCK_RADIO
		return true;
#else
		if (!LoLaSi446xRadioDriver::RadioSetup())
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Si4463 Present"));
			Serial.println(DeviceInfo.RevisionBranch);
			Serial.println(DeviceInfo.RevisionExternal);
			Serial.println(DeviceInfo.RevisionInternal);
			Serial.println(DeviceInfo.ChipRevision);
			Serial.println(DeviceInfo.Customer);
			Serial.println(DeviceInfo.PartId);
			Serial.println(DeviceInfo.Patch);
			Serial.println(DeviceInfo.PartBuild);
			Serial.println(DeviceInfo.Function);
			Serial.println(DeviceInfo.ROM);
#endif // DEBUG_LOLA
			return true;
		}
		else
		{
			return false;
		}
#endif
	}

private:
	// Return true if done.
	bool CheckQueue(const uint32_t timeMicros)
	{
		ReadInterruptFlags();

		// Valid PREAMBLE and SYNC, packet data now begins
		if (HasInterruptSyncDetect())
		{
			LastReceivedInfo.Micros = timeMicros;
			LastReceivedInfo.RSSI = GetLatchedRSSI();
			OnIncoming();
		}
		// Valid packet.
		else if (HasInterruptRX())
		{
			uint8_t PacketLength = GetReceivedLength();

			if (PacketLength >= LOLA_PACKET_MIN_PACKET_SIZE &&
				PacketLength < LOLA_PACKET_MAX_PACKET_SIZE &&
				AllowedReceive())
			{
				ReadReceived(PacketLength);
				if (OnReceiveOk(PacketLength))
				{
					SetRadioReceive();
				}
			}
			else
			{
				OnReceiveFail();
				SetRadioReceive();
			}
		}
		// Internal CRC is not used.
		// Corrupted packet.
		else if (HasInterruptCRCError())
		{
			SetRadioReceive();
			OnReceiveFail();
		}
		// Packet sent.
		else if (HasInterruptPacketSent())
		{
			if (OnSentOk(timeMicros))
			{
				SetRadioReceive();
			}
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

	void SetRadioReceive()
	{
		if (AllowedRadioChange() && SetRadioReceiveInternal())
		{
			PendingActions.ClearRX();
		}
		else
		{
			PendingActions.SetRX();
			Wake();
		}
	}
};
#endif