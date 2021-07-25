// LoLaSi446xPacketDriver.h

#ifndef _LOLA_SI446X_ZAKKEMBLE_WRAPPER_PACKET_DRIVER_h
#define _LOLA_SI446X_ZAKKEMBLE_WRAPPER_PACKET_DRIVER_h


#include <PacketDriver\LoLaAsyncPacketDriver.h>
#include <SPI.h>
#include <Si446x.h>


class Si446xWrapperInterface
{
public:
	virtual	void OnRadioReceivedOk(const uint8_t length) {}
	virtual	void OnRadioIncoming(const int16_t rssi, const uint32_t timestamp) {}
	virtual	void OnRadioReceiveFail() {}
	virtual	void OnRadioSent() {}
};

class LoLaSi446xPacketDriver final
	: public LoLaAsyncPacketDriver, public virtual Si446xWrapperInterface
{
private:
	//Expected part number.
	static const uint16_t PART_NUMBER_SI4463X = 17507;

	//Si446xSettings Settings;
	LoLaPacketDriverSettings Si446xDriverSettings; // (1000, -20, -80, 10, 0, 127, 1);

public:
	LoLaSi446xPacketDriver(Scheduler* scheduler)
		: Si446xDriverSettings(1000, -20, -80, 10, 0, 10, 1)
		, LoLaAsyncPacketDriver(scheduler, &Si446xDriverSettings)
	{
	}
public:
	virtual void OnRadioIncoming(const int16_t rssi, const uint32_t timestamp)
	{
		Serial.println('.');
		if (DriverState == DriverActiveState::DriverDisabled)
		{
			return;
		}
		OnIncoming(rssi, timestamp);
	}

	virtual void OnRadioReceivedOk(const uint8_t length)
	{
		Serial.println('@');
		if (DriverState == DriverActiveState::DriverDisabled)
		{
			return;
		}

		RequestDriverAction(length);
	}

	virtual void OnRadioReceiveFail()
	{
		Serial.println('?');
		if (DriverState == DriverActiveState::DriverDisabled)
		{
			return;
		}
		InvalidateChannel();
	}

	virtual void OnRadioSent()
	{
		Serial.println('!');
		if (DriverState == DriverActiveState::DriverDisabled)
		{
			return;
		}
		InvalidateChannel();
	}

public:
	virtual bool Setup()
	{
		if (LoLaAsyncPacketDriver::Setup())
		{
			SetupInterrupts();

			SPI.setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz

			// Start up
			Si446x_init();
			si446x_info_t info;
			Si446x_getInfo(&info);


			if (info.part == PART_NUMBER_SI4463X)
			{
				Si446x_setupCallback(SI446X_CBS_RXBEGIN | SI446X_CBS_SENT, 1);
				Si446x_sleep();
#ifdef DEBUG_LOLA
				Serial.println(F("Si4463 Present"));
				//Serial.println(info.revBranch);
				//Serial.println(info.revExternal);
				//Serial.println(info.revInternal);
				//Serial.println(info.chipRev);
				//Serial.println(info.customer);
				//Serial.println(info.id);
				//Serial.println(info.patch);
				//Serial.println(info.partBuild);
				//Serial.println(info.func);
				//Serial.println(info.romId);
#endif // DEBUG_LOLA
				return true;
			}
			else
			{
				Si446x_sleep();
#ifdef DEBUG_LOLA
				Serial.print(F("Part number invalid: "));
				Serial.println(info.part);
				Serial.println(F("Si4463 Driver failed to start."));
#endif // DEBUG_LOLA
			}
		}

		return false;
	}

	virtual void Start()
	{
		LoLaAsyncPacketDriver::Start();
		InvalidateChannel();
	}

	virtual void Stop()
	{
		LoLaAsyncPacketDriver::Stop();
		Si446x_sleep();
	}

	// Restore to RX and call OnRadioSetToRX,
	// regardless if successfull.
	virtual bool RadioTransmit(const uint8_t length)
	{
		Serial.print(F("Sent Packet ("));
		Serial.print(length);
		Serial.print(F(") 0x|"));
		for (uint8_t i = 0; i < length; i++)
		{
			Serial.print(OutgoingPacket.GetRaw()[i], HEX);
			Serial.print('|');
		}
		Serial.println();

		return 1 == Si446x_TX((uint8_t*)OutgoingPacket.GetRaw(), length, GetChannel(), SI446X_STATE_NOCHANGE);
	}

	virtual bool SetRadioPower()
	{
		Si446x_setTxPower(GetTransmitPower());

		return true;
	}

	virtual bool SetRadioChannel()
	{
		return SetRadioReceive();
	}

	virtual bool SetRadioReceive()
	{
		Si446x_RX(GetChannel());
		OnRadioSetToRX();

		return true;
	}

protected:
	virtual void DriverCallback(const uint8_t action)
	{
		if (action >= PacketDefinition::LOLA_PACKET_MIN_PACKET_TOTAL_SIZE)
		{
			Si446x_read((uint8_t*)IncomingPacket.GetRaw(), action);
			Serial.print(F("Got packet ("));
			Serial.print(action);
			Serial.print(F(") 0x|"));

			for (uint8_t i = 0; i < action; i++)
			{
				Serial.print(IncomingPacket.GetRaw()[i], HEX);
				Serial.print('|');
			}
			Serial.println();
			OnReceiveOk(action);
			SetRadioReceive();
		}
		else
		{
			OnRadioReceiveFail();
		}
	}

private:
	void SetupInterrupts();

};
#endif