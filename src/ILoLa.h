// ILoLa.h

#ifndef _ILOLA_h
#define _ILOLA_h

#define DEBUG_LOLA

//#define MOCK_RADIO
//#define USE_MOCK_PACKET_LOSS

#define USE_TIME_SLOT
#define USE_LATENCY_COMPENSATION
#define USE_ENCRYPTION
//#define USE_FREQUENCY_HOP

//#define DEBUG_LOLA_CRYPTO

#include <stdint.h>
#include <Transceivers\LoLaReceiver.h>
#include <Transceivers\LoLaSender.h>

#ifdef DEBUG_LOLA
#define LOLA_LINK_DEBUG_UPDATE_SECONDS						60
#endif

// 1000 Relaxed period.
// 500 Recommended period.
// 50 Minimum recommended period.
#define LOLA_LINK_FREQUENCY_HOP_PERIOD_MILLIS				500


// 1000 Change TOTP every second, low chance of sync error.
// 100 Agressive crypto denial.
#define ILOLA_CRYPTO_TOTP_PERIOD_MILLIS						1000

// 100 High latency, high bandwitdh.
// 10 Low latency, lower bandwidth.
#define ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS					10

// Packet collision avoidance.
#define LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS			2

// 
#define ILOLA_TRANSMIT_EXPIRY_PERIOD_MIN_MILLIS				2
#define ILOLA_TRANSMIT_EXPIRY_PERIOD_MILLIS_100_BYTE		60


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
#include <LoLaCrypto\ISeedSource.h>
#include <LoLaCrypto\LoLaCryptoTokenSource.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


#include <ClockSource.h>

class ILoLa
{
protected:
	struct InputInfoType
	{
		volatile uint32_t Time = ILOLA_INVALID_MILLIS;
		volatile int16_t RSSI = ILOLA_INVALID_RSSI;

		void Clear()
		{
			Time = ILOLA_INVALID_MILLIS;
			RSSI = ILOLA_INVALID_RSSI;
		}
	};

	struct OutputInfoType
	{
		volatile uint32_t Time = ILOLA_INVALID_MILLIS;

		void Clear()
		{
			Time = ILOLA_INVALID_MILLIS;
		}
	};

	///Statistics
	InputInfoType LastReceivedInfo;
	OutputInfoType LastSentInfo;
	
	InputInfoType LastValidReceivedInfo;
	OutputInfoType LastValidSentInfo;

	uint32_t TransmitedCount = 0;
	uint32_t ReceivedCount = 0;
	uint32_t RejectedCount = 0;
	///

	///Configurations
	//From 0 to UINT8_MAX, limited by driver.
	uint8_t CurrentTransmitPower = ILOLA_DEFAULT_TRANSMIT_POWER;
	uint8_t CurrentChannel = ILOLA_DEFAULT_CHANNEL;
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

	///Crypto
	LoLaCryptoTokenSource	CryptoToken;
	LoLaCryptoEncoder		CryptoEncoder;
	///

	///For use of estimated latency features
	uint8_t ETTM = 0;//Estimated transmission time in millis.
	///

	enum DriverActiveStates :uint8_t
	{
		DriverDisabled,
		ReadyForAnything,
		BlockedForIncoming,
		AwaitingProcessing,
		ProcessingIncoming,
		SendingOutgoing,
		SendingAck,
		WaitingForTransmissionEnd
	};

	

public:
	ILoLa() : PacketMap(), SyncedClock()
	{
	}

	LoLaCryptoTokenSource* GetCryptoSeed()
	{
		return &CryptoToken;
	}

	ISeedSource* GetCryptoTokenSource()
	{
		return &CryptoToken;
	}

	LoLaCryptoEncoder* GetCryptoEncoder()
	{
		return &CryptoEncoder;
	}

	void Enable()
	{
		OnStart();
	}

	void Disable()
	{
		OnStop();
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


	uint8_t GetETTM()
	{
		return ETTM;
	}

	void SetETTM(const uint8_t ettm)
	{
#ifdef USE_LATENCY_COMPENSATION
		ETTM = ettm;
#else
		ETTM = 0;
#endif
		Sender.SetETTM(ETTM);

	}


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
		ETTM = 0;
		Sender.SetETTM(ETTM);

		LastReceivedInfo.Clear();
		LastSentInfo.Clear();

		LastValidReceivedInfo.Clear();
		LastValidSentInfo.Clear();

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

	int16_t GetLastRSSI()
	{
		return LastReceivedInfo.RSSI;
	}

	uint32_t GetLastValidReceivedMillis()
	{
		return LastValidReceivedInfo.Time;
	}

	uint32_t GetLastValidSentMillis()
	{
		return LastValidSentInfo.Time;
	}

	int16_t GetLastValidRSSI()
	{
		return LastValidReceivedInfo.RSSI;
	}

	uint8_t GetTransmitPowerNormalized()
	{
		return map(CurrentTransmitPower, GetTransmitPowerMin(), GetTransmitPowerMax(), 0, UINT8_MAX);
	}

	void SetTransmitPower(const uint8_t transmitPowerNormalized)
	{
		CurrentTransmitPower = map(transmitPowerNormalized, 0, UINT8_MAX, GetTransmitPowerMin(), GetTransmitPowerMax());

		OnTransmitPowerUpdated();
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

protected:
	//Driver constants' overload.
	virtual uint8_t GetTransmitPowerMax() const { return 1; }
	virtual uint8_t GetTransmitPowerMin() const { return 0; }

	virtual int16_t GetRSSIMax() const { return 0; }
	virtual int16_t GetRSSIMin() const { return ILOLA_DEFAULT_MIN_RSSI; }

public:
	//Packet driver implementation.
	virtual bool SendPacket(ILoLaPacket* packet) { return false; }
	virtual bool Setup() { return true; }
	virtual bool AllowedSend(const bool overridePermission = false) { return true; }
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void OnChannelUpdated() {}
	virtual void OnTransmitPowerUpdated() {}

	//Device driver implementation.
	virtual uint8_t GetChannelMax() const { return 0; }
	virtual uint8_t GetChannelMin() const { return 0; }

	//Device driver events.
	virtual void OnWakeUpTimer() {}
	virtual void OnBatteryAlarm() {}
	virtual void OnReceiveBegin(const uint8_t length, const int16_t rssi) {}
	virtual void OnReceivedFail(const int16_t rssi) {}
	virtual void OnSentOk() {}
	virtual void OnIncoming(const int16_t rssi) {}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);
	}
#endif
	};
#endif