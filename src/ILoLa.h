// ILoLa.h

#ifndef _ILOLA_h
#define _ILOLA_h

#define DEBUG_LOLA
#define MOCK_RADIO

#define ILOLA_DEFAULT_CHANNEL 20

#define ILOLA_DEFAULT_MIN_RSSI (int16_t(-100))

#if !defined(UINT16_MAX) || !defined(INT16_MIN) || !defined(UINT32_MAX) || defined(UINT8_MAX)
#include <stdint.h>
#endif

#define ILOLA_INVALID_RSSI		((int16_t)INT16_MIN)
#define ILOLA_INVALID_MILLIS	((uint32_t)UINT32_MAX)
#define ILOLA_INVALID_LATENCY	((uint16_t)UINT16_MAX)

#include <Arduino.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>


class ILoLa
{
protected:
	///Statistics
	volatile uint32_t LastSent = ILOLA_INVALID_MILLIS;
	volatile uint32_t LastReceived = ILOLA_INVALID_MILLIS;
	volatile int16_t LastReceivedRssi = ILOLA_INVALID_RSSI;
	///

	///Configurations
	uint8_t TransmitPower = 0;
	uint8_t CurrentChannel = ILOLA_DEFAULT_CHANNEL;
	bool SendPermission = true;
	bool Enabled = false;
	///

	///Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;
	///

public:
	ILoLa()
	{
	}

	void Enable()
	{
		if (!Enabled)
		{
			OnStart();
		}
		Enabled = true;
	}

	void Disable()
	{
		Enabled = false;
	}

	uint32_t GetTimeStamp()
	{
		return micros();
	}

	uint32_t GetMillis()
	{
		return millis();
	}

	uint32_t GetLastSentMillis()
	{
		return LastSent;
	}

	uint32_t GetLastReceivedMillis()
	{
		return LastReceived;
	}

	int16_t GetLastRSSI()
	{
		return LastReceivedRssi;
	}

	uint8_t GetTransmitPower()
	{
		return TransmitPower;
	}

	virtual bool SetTransmitPower(const uint8_t transmitPower)
	{
		TransmitPower = transmitPower;

		return true;
	}

	uint8_t GetChannel()
	{
		return CurrentChannel;
	}

	virtual bool SetChannel(const int16_t channel)
	{
		CurrentChannel = channel;

		return true;
	}

	LoLaPacketMap * GetPacketMap()
	{
		return &PacketMap;
	}

protected:

	uint8_t * GetTransmitPowerPointer()
	{
		return &TransmitPower;
	}

	uint8_t* GetChannelPointer()
	{
		return &CurrentChannel;
	}

public:
	virtual bool SendPacket(ILoLaPacket* packet) { return false; }
	virtual bool Setup() { return true; }
	virtual bool AllowedSend() { return true; }
	virtual void OnStart() {}
	virtual uint32_t GetLastValidReceivedMillis() { return  LastReceived; }
	virtual int16_t GetLastValidRSSI() { return LastReceivedRssi; }
	virtual uint8_t GetTransmitPowerMax() { return 0xFF; }
	virtual uint8_t GetTransmitPowerMin() { return 0; }
	virtual int16_t GetRSSIMax() { return 0; }
	virtual int16_t GetRSSIMin() { return ILOLA_DEFAULT_MIN_RSSI; }

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);
	}
#endif
	///Public static members to cross the static events to instance.

public:
	virtual void OnWakeUpTimer() {}
	virtual void OnReceiveBegin(const uint8_t length, const  int16_t rssi) {}
	virtual void OnReceivedFail(const int16_t rssi) {}
	virtual void OnReceived() {}
	virtual void OnSentOk() {}
	virtual void OnIncoming(const int16_t rssi) {}

	virtual void OnBatteryAlarm() {}
	///
};
#endif

