// ILoLa.h

#ifndef _ILOLA_h
#define _ILOLA_h

#define DEBUG_LOLA
//#define MOCK_RADIO
//#define USE_MOCK_PACKET_LOSS
#define USE_TIME_SLOT
#define USE_LATENCY_COMPENSATION

#if !defined(UINT16_MAX) || !defined(INT16_MIN) || !defined(UINT32_MAX) || defined(UINT8_MAX)
#include <stdint.h>
#endif

#define ILOLA_DEFAULT_CHANNEL				0
#define ILOLA_DEFAULT_TRANSMIT_POWER		10
#define ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS	10

#define ILOLA_DEFAULT_MIN_RSSI				(int16_t(-100))

#define ILOLA_INVALID_RSSI					((int16_t)INT16_MIN)
#define ILOLA_INVALID_RSSI_NORMALIZED		((uint8_t)0)
#define ILOLA_INVALID_MILLIS				((uint32_t)UINT32_MAX)
#define ILOLA_INVALID_MICROS				ILOLA_INVALID_MILLIS
#define ILOLA_INVALID_LATENCY				((uint16_t)UINT16_MAX)

#ifdef USE_MOCK_PACKET_LOSS
#define MOCK_PACKET_LOSS_SOFT				8
#define MOCK_PACKET_LOSS_HARD				12
#define MOCK_PACKET_LOSS_SMOKE_SIGNALS		35
#define MOCK_PACKET_LOSS_LINKING			MOCK_PACKET_LOSS_HARD
#define MOCK_PACKET_LOSS_LINKED				MOCK_PACKET_LOSS_SMOKE_SIGNALS
#endif

#include <Arduino.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Crypto\ISeedSource.h>

#include <ClockSource.h>

class ILoLa
{
protected:
	///Statistics
	volatile uint32_t LastSent = ILOLA_INVALID_MILLIS;
	volatile uint32_t LastReceived = ILOLA_INVALID_MILLIS;
	volatile int16_t LastReceivedRssi = ILOLA_INVALID_RSSI;

	uint32_t LastValidReceived = ILOLA_INVALID_MILLIS;
	int16_t LastValidReceivedRssi = ILOLA_INVALID_RSSI;
	///

	///Configurations
	//From 0 to UINT8_MAX, limited by driver.
	uint8_t CurrentTransmitPower = ILOLA_DEFAULT_TRANSMIT_POWER;
	uint8_t CurrentChannel = ILOLA_DEFAULT_CHANNEL;
	bool Enabled = false;
	const uint8_t DuplexPeriodMillis = ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS;
	///

	///Status
#ifdef USE_TIME_SLOT
	bool LinkActive = false;
	bool EvenSlot = false;
	uint32_t SendSlotElapsed;
#endif
	///

	///Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;
	///

	///Synced clock
	ClockSource SyncedClock;
	///


	///For use of estimated latency features
#ifdef USE_LATENCY_COMPENSATION
	uint8_t ETTM = 0;//Estimated transmission time in millis.
#endif
	///

public:
	ILoLa() : PacketMap(), SyncedClock()
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
		if (Enabled)
		{
			OnStop();
		}
		Enabled = false;
	}

	void SetLinkStatus(const bool linked)
	{
		LinkActive = linked;
	}

	bool HasLink()
	{
		return LinkActive;
	}

#ifdef USE_MOCK_PACKET_LOSS
	bool GetLossChance()
	{
		if (LinkIndicator->HasLink())
		{
			return random(100) + 1 >= MOCK_PACKET_LOSS_LINKED;
		}
		else
		{
			return random(100) + 1 >= MOCK_PACKET_LOSS_LINKING;

		}
	}
#endif

#ifdef USE_LATENCY_COMPENSATION
	uint8_t GetETTM()
	{
		return ETTM;
	}

	void SetETTM(const uint8_t ettm)
	{
		ETTM = ettm;
		OnETTMUpdated();
	}

	void ResetETTM()
	{
		ETTM = 0;
		OnETTMUpdated();
	}
#endif

#ifdef USE_TIME_SLOT
	void SetDuplexSlot(const bool evenSlot)
	{
		EvenSlot = evenSlot;
	}
#endif

	ClockSource* GetClockSource()
	{
		return &SyncedClock;
	}

	uint32_t GetTimeStamp()
	{
		return SyncedClock.GetMicros();
	}

	uint32_t GetMillis()
	{
		return millis();
	}

	uint32_t GetMillisSync()
	{
		return SyncedClock.GetMillis();
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

	uint32_t GetLastValidReceivedMillis()
	{
		return  LastValidReceived;
	}

	int16_t GetLastValidRSSI()
	{
		return LastValidReceivedRssi;
	}

	uint8_t GetTransmitPower()
	{
		return CurrentTransmitPower;
	}

	bool SetTransmitPower(const uint8_t transmitPower)
	{
		CurrentTransmitPower = transmitPower;

		OnTransmitPowerUpdated();

		return true;
	}

	uint8_t GetChannel()
	{
		return CurrentChannel;
	}

	bool SetChannel(const int16_t channel)
	{
		CurrentChannel = channel;

		OnChannelUpdated();

		return true;
	}

	LoLaPacketMap * GetPacketMap()
	{
		return &PacketMap;
	}

protected:
#ifdef USE_LATENCY_COMPENSATION
	virtual void OnETTMUpdated() {}
#endif

public:
	virtual bool SendPacket(ILoLaPacket* packet) { return false; }
	virtual bool Setup() { return true; }
	virtual bool AllowedSend(const bool overridePermission = false) { return true; }
	virtual void OnStart() {}
	virtual void OnStop() {}

	virtual int16_t GetRSSIMax() const { return 0; }
	virtual int16_t GetRSSIMin() const { return ILOLA_DEFAULT_MIN_RSSI; }
	virtual uint8_t GetChannelMax() const { return 0; }
	virtual uint8_t GetChannelMin() const { return 0; }
	virtual uint8_t GetTransmitPowerMax() const { return 0; }
	virtual uint8_t GetTransmitPowerMin() const { return 0; }

	virtual void SetCryptoSeedSource(ISeedSource* cryptoSeedSource) {}

	virtual void OnWakeUpTimer() {}
	virtual void OnReceiveBegin(const uint8_t length, const  int16_t rssi) {}
	virtual void OnReceivedFail(const int16_t rssi) {}
	virtual void OnReceived() {}
	virtual void OnSentOk() {}
	virtual void OnIncoming(const int16_t rssi) {}


	virtual void OnChannelUpdated() {}
	virtual void OnTransmitPowerUpdated() {}

	virtual void OnBatteryAlarm() {}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);
	}
#endif
};
#endif