// ILoLa.h

#ifndef _ILOLA_h
#define _ILOLA_h

#define DEBUG_LOLA
//#define MOCK_RADIO
//#define USE_MOCK_PACKET_LOSS
#define USE_TIME_SLOT
#define USE_LATENCY_COMPENSATION
#define USE_FREQUENCY_HOP

#include <stdint.h>
#include <Transceivers\LoLaReceiver.h>
#include <Transceivers\LoLaSender.h>

#ifdef DEBUG_LOLA
#define LOLA_LINK_DEBUG_UPDATE_SECONDS 10
#endif

// 1000 Relaxed period.
// 500 Recommended period.
// 50 Minimum recommended period.
#define LOLA_LINK_FREQUENCY_HOP_PERIOD_MILLIS 500


// 1000 Change TOTP every second, low chance of sync error.
// 100 Agressive crypto denial.
#define ILOLA_CRYPTO_TOTP_PERIOD_MILLIS		(uint32_t)(1000)

// 100 High latency, high bandwitdh.
// 10 Low latency, lower bandwidth.
#define ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS	10

// Packet collision avoidance before link.
#define LOLA_LINK_UNLINK_SEND_BACK_OFF_DURATION_MILLIS 8
#define LOLA_LINK_UNLINK_RECEIVE_BACK_OFF_DURATION_MILLIS 4

// Packet collision avoidance with link.
#define LOLA_LINK_LINKED_SEND_BACK_OFF_DURATION_MILLIS 4
#define LOLA_LINK_LINKED_RECEIVE_BACK_OFF_DURATION_MILLIS 2


///Driver constants.
#define ILOLA_DEFAULT_CHANNEL				0
#define ILOLA_DEFAULT_TRANSMIT_POWER		10

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
///

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

	uint32_t TransmitedCount = 0;
	uint32_t ReceivedCount = 0;
	uint32_t RejectedCount = 0;
	///

	///Configurations
	//From 0 to UINT8_MAX, limited by driver.
	uint8_t CurrentTransmitPower = ILOLA_DEFAULT_TRANSMIT_POWER;
	uint8_t CurrentChannel = ILOLA_DEFAULT_CHANNEL;
	bool Enabled = false;
	const uint8_t DuplexPeriodMillis = ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS;
	///

	///Transceivers.
	LoLaReceiver Receiver;
	LoLaSender Sender;
	///

	///Status
	bool LinkActive = false;

#ifdef USE_TIME_SLOT
	bool EvenSlot = false;
	//Helper.
	uint32_t SendSlotElapsed;
#endif
	///

	///Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;
	///

	///Synced clock
	ClockSource SyncedClock;
	///

#ifdef USE_LATENCY_COMPENSATION
	///For use of estimated latency features
	uint8_t ETTM = 0;//Estimated transmission time in millis.
	///
#endif

public:
	ILoLa() : PacketMap(), SyncedClock()
	{
	}

	void SetCryptoSeedSource(ISeedSource* cryptoSeedSource)
	{
		Sender.SetCryptoSeedSource(cryptoSeedSource);
		Receiver.SetCryptoSeedSource(cryptoSeedSource);
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
		if (LinkActive)
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
		Sender.SetETTM(ETTM);
	}
#endif

#ifdef USE_TIME_SLOT
	void SetDuplexSlot(const bool evenSlot)
	{
		EvenSlot = evenSlot;
	}
#endif

	void ResetStatistics()
	{
		TransmitedCount = 0;
		ReceivedCount = 0;
		RejectedCount = 0;
	}

	void ResetLiveData()
	{
#ifdef USE_LATENCY_COMPENSATION
		ETTM = 0;
		Sender.SetETTM(ETTM);
#endif
		LastSent = ILOLA_INVALID_MILLIS;
		LastReceived = ILOLA_INVALID_MILLIS;
		LastReceivedRssi = ILOLA_INVALID_RSSI;

		LastValidReceived = ILOLA_INVALID_MILLIS;
		LastValidReceivedRssi = ILOLA_INVALID_RSSI;

		ResetStatistics();
	}

	ClockSource* GetClockSource()
	{
		return &SyncedClock;
	}

	uint8_t GetRSSINormalized()
	{
		return (uint8_t)map(constrain(GetLastValidRSSI(), GetRSSIMin(), GetRSSIMax()),
			GetRSSIMin(), GetRSSIMax(),
			0, UINT8_MAX);
	}

	uint32_t GetSyncMillis()
	{
		return SyncedClock.GetSyncMillis();
	}

	uint32_t GetReceivedCount()
	{
		return ReceivedCount;
	}

	uint32_t GetRejectedCount()
	{
		return RejectedCount;
	}

	uint32_t GetSentCount()
	{
		return TransmitedCount;
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
		return LastValidReceived;
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

	bool SetChannel(const uint8_t channel)
	{
		CurrentChannel = channel;

		OnChannelUpdated();

		return true;
	}

	LoLaPacketMap * GetPacketMap()
	{
		return &PacketMap;
	}

public:
	//Packet driver implementation.
	virtual bool SendPacket(ILoLaPacket* packet) { return false; }
	virtual bool Setup() { return true; }
	virtual bool AllowedSend(const bool overridePermission = false) { return true; }
	virtual void OnStart() {}
	virtual void OnStop() {}


	//Device driver implementation.
	virtual int16_t GetRSSIMax() const { return 0; }
	virtual int16_t GetRSSIMin() const { return ILOLA_DEFAULT_MIN_RSSI; }
	virtual uint8_t GetChannelMax() const { return 0; }
	virtual uint8_t GetChannelMin() const { return 0; }
	virtual uint8_t GetTransmitPowerMax() const { return 0; }
	virtual uint8_t GetTransmitPowerMin() const { return 0; }

	//Device driver events.
	virtual void OnWakeUpTimer() {}
	virtual void OnBatteryAlarm() {}
	virtual void OnReceiveBegin(const uint8_t length, const  int16_t rssi) {}
	virtual void OnReceivedFail(const int16_t rssi) {}
	virtual void OnReceived() {}
	virtual void OnSentOk() {}
	virtual void OnIncoming(const int16_t rssi) {}

	//Device driver overloads.
	virtual void OnChannelUpdated() {}
	virtual void OnTransmitPowerUpdated() {}
	
#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);
	}
#endif
};
#endif