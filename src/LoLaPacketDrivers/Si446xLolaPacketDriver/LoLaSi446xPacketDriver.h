//// LoLaSi446xPacketDriver.h
//
//#ifndef _LOLASI446XPACKETDRIVER_h
//#define _LOLASI446XPACKETDRIVER_h
//
//
//#include <ILoLaPacketDriver.h>
//
//#include <PacketDriver\LoLaSi446x\LoLaSi446xRadioDriver.h>
//#include <PacketDriver\LoLaSi446x\PendingActionsTracker.h>
//
//
//class LoLaSi446xPacketDriver
//	: public LoLaSi446xRadioDriver
//	, public virtual ILoLaPacketDriver
//{
//private:
//	static const uint32_t RESPONSE_TIMEOUT_MICROS = 500000;
//	static const uint32_t RESPONSE_SHORT_TIMEOUT_MICROS = 250;
//	static const uint32_t RESPONSE_CHECK_PERIOD_MICROS = 2;
//
//	uint8_t InterruptFlags[8];
//
//	uint32_t LastEventTimestamp = 0;
//
//	//PendingActionsTracker PendingActions;
//
//public:
//	LoLaSi446xPacketDriver(SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
//		: LoLaSi446xRadioDriver(spi, cs, reset, irq)
//		, ILoLaPacketDriver()
//	{
//	}
//
//#ifdef DEBUG_LOLA
//	virtual void Debug(Stream* serial)
//	{
//		LoLaPacketDriver::Debug(serial);
//
//		serial->print(F("Driver timeouts: "));
//		serial->println(TimeoutCount);
//	}
//#endif
//
//	//Return true when there is no more work to be done.
//	bool CheckPending()
//	{
//		//Only update event timestamp if an interrupt hasn't touched it.
//		if (LastEventTimestamp == 0 ||
//			LastEventTimestamp == EventTimestamp)
//		{
//			EventTimestamp = micros();
//		}
//
//		LastEventTimestamp = EventTimestamp;
//		if (CheckQueue(LastEventTimestamp))
//		{
//			if (PendingActions.GetRX())
//			{
//				SetRadioReceive();
//			}
//			else if (PendingActions.GetPower())
//			{
//				SetRadioPower();
//			}
//			else if (PendingActions.GetChannel())
//			{
//				SetRadioChannel();
//			}
//			else
//			{
//				return true;
//			}
//		}
//
//		return false;
//	}
//
//protected:
//	virtual void OnResetLiveData()
//	{
//		PendingActions.Clear();
//	}
//
//	virtual void OnChannelUpdated()
//	{
//		PendingActions.SetChannel();
//		Wake();
//	}
//
//	virtual void OnTransmitPowerUpdated()
//	{
//		PendingActions.SetPower();
//		Wake();
//	}
//
//	void SetRadioPower()
//	{
//		if (AllowedRadioChange() &&
//			SetProperty(CONFIG_PA_POWER, GetTransmitPower()))
//		{
//			PendingActions.ClearPower();
//		}
//		else
//		{
//			PendingActions.SetPower();
//			Wake();
//		}
//	}
//
//	void SetRadioChannel()
//	{
//		if (AllowedRadioChange())
//		{
//			SetRadioReceive();
//			PendingActions.ClearChannel();
//		}
//		else
//		{
//			PendingActions.SetChannel();
//			Wake();
//		}
//	}
//
//	bool RadioTransmit()
//	{
//#ifdef LOLA_MOCK_RADIO
//		delayMicroseconds(500);
//		return true;
//#else
//
//		if (!AllowedRadioChange())
//		{
//			return false;
//		}
//
//		return RadioTransmitInternal();
//#endif
//	}
//
//	bool RadioSetup()
//	{
//#ifdef LOLA_MOCK_RADIO
//		return true;
//#else
//		if (!LoLaSi446xRadioDriver::RadioSetup())
//		{
//			return false;
//		}
//
//		Serial.println(F("RadioSetup2"));
//		ApplyStartupConfig();
//
//		ReadInfo();
//
//		if (DeviceInfo.PartId == PART_NUMBER_SI4463X)
//		{
//			SetupCallbacks(); // Enable packet RX begin and packet sent callbacks.
//			SetRadioPower();
//			SetBatteryWarningThreshold(3200); // Set low battery voltage to 3200mV.
//
//			SetupWUT(1, 8192); // Run check battery every 2 seconds.
//
//			SleepRadio();
//
//			// Start up.
//			InterruptsEnabled = false;
//			InterruptOn();
//
//#ifdef DEBUG_LOLA
//			//Serial.println(F("Si4463 Present"));
//			//Serial.println(DeviceInfo.revBranch);
//			//Serial.println(DeviceInfo.revExternal);
//			//Serial.println(DeviceInfo.revInternal);
//			//Serial.println(DeviceInfo.chipRev);
//			//Serial.println(DeviceInfo.customer);
//			//Serial.println(DeviceInfo.id);
//			//Serial.println(DeviceInfo.patch);
//			//Serial.println(DeviceInfo.partBuild);
//			//Serial.println(DeviceInfo.func);
//			//Serial.println(DeviceInfo.romId);
//#endif // DEBUG_LOLA
//			return true;
//		}
//		else
//		{
//#ifdef DEBUG_LOLA
//			Serial.print(F("Part number invalid: "));
//			Serial.println(DeviceInfo.PartId);
//			Serial.println(F("Si4463 Driver failed to start."));
//#endif // DEBUG_LOLA
//			return false;
//		}
//#endif
//	}
//
//private:
//	// Return true if done.
//	bool CheckQueue(const uint32_t timeMicros)
//	{
//		ReadInterruptFlags();
//
//		// Valid PREAMBLE and SYNC, packet data now begins
//		if (InterruptFlags[4] & PENDING_EVENT_SYNC_DETECT)
//		{
//			LastReceivedInfo.Micros = timeMicros;
//			LastReceivedInfo.RSSI = GetLatchedRSSI();
//			OnIncoming();
//		}
//		// Valid packet.
//		else if (InterruptFlags[2] & PENDING_EVENT_RX)
//		{
//			uint8_t PacketLength = GetReceivedLength();
//
//			if (PacketLength >= LOLA_PACKET_MIN_PACKET_SIZE &&
//				PacketLength < LOLA_PACKET_MAX_PACKET_SIZE &&
//				AllowedReceive())
//			{
//				ReadReceived(PacketLength);
//				if (OnReceiveOk(PacketLength))
//				{
//					SetRadioReceive();
//				}
//			}
//			else
//			{
//				OnReceiveFail();
//				SetRadioReceive();
//			}
//		}
//		// Internal CRC is not used.
//		// Corrupted packet.
//		else if (InterruptFlags[2] & PENDING_EVENT_CRC_ERROR)
//		{
//			SetRadioReceive();
//			OnReceiveFail();
//		}
//		// Packet sent.
//		else if (InterruptFlags[2] & PENDING_EVENT_SENT_OK)
//		{
//			if (OnSentOk(timeMicros))
//			{
//				SetRadioReceive();
//			}
//		}
//		else if (InterruptFlags[6] & PENDING_EVENT_LOW_BATTERY)
//		{
//			OnBatteryAlarm();
//		}
//		else if (InterruptFlags[6] & (1 << PENDING_EVENT_WUT))
//		{
//			return true;//Not used.
//		}
//		else
//		{
//			return true;
//		}
//
//		return false;
//	}
//
//	void SetRadioReceive()
//	{
//		if (AllowedRadioChange() && SetRadioReceiveInternal())
//		{
//			PendingActions.ClearRX();
//		}
//		else 
//		{
//			PendingActions.SetRX();
//			Wake();
//		}
//	}
//
//	void ReadInterruptFlags()
//	{
//		if (WaitForResponse(MessageIn, 0, RESPONSE_SHORT_TIMEOUT_MICROS)) // Make sure it's ok to send a command
//		{
//			RADIO_ATOMIC()
//			{
//				CHIP_SELECT()
//				{
//					MessageOut[0] = COMMAND_GET_STATUS;
//					spi->transfer(MessageOut[0]);
//				}
//			}
//
//			WaitForResponse(InterruptFlags, sizeof(InterruptFlags), RESPONSE_SHORT_TIMEOUT_MICROS);
//		}
//	}
//};
//#endif