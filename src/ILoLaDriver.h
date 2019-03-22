// ILoLaDriver.h

#ifndef _ILOLA_h
#define _ILOLA_h

#include <stdint.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>
#include <ClockSource.h>
#include <LoLaDefinitions.h>

class ILoLaDriver
{
protected:
	struct InputInfoType
	{
		volatile uint32_t Micros = ILOLA_INVALID_MICROS;
		volatile int16_t RSSI = ILOLA_INVALID_RSSI;

		void Clear()
		{

			Micros = ILOLA_INVALID_MICROS;
			RSSI = ILOLA_INVALID_RSSI;
		}
	};

	struct OutputInfoType
	{
		volatile uint32_t Micros = ILOLA_INVALID_MICROS;

		void Clear()
		{
			Micros = ILOLA_INVALID_MICROS;
		}
	};

	volatile uint8_t LastSentHeader = 0xff;

	///Statistics
	InputInfoType LastReceivedInfo;
	OutputInfoType LastSentInfo;

	InputInfoType LastValidReceivedInfo;
	OutputInfoType LastValidSentInfo;

	uint64_t TransmitedCount = 0;
	uint64_t ReceivedCount = 0;
	uint64_t RejectedCount = 0;
	uint64_t TimingCollisionCount = 0;
	///

	///Configurations
	//From 0 to UINT8_MAX, limited by driver.
	uint8_t CurrentTransmitPower = 0;
	uint8_t CurrentChannel = 0;
	const uint32_t DuplexPeriodMicros = ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS * (uint32_t)1000;
	const uint32_t HaldDuplexPeriodMicros = DuplexPeriodMicros / 2;
	const uint32_t BackOffPeriodMicros = LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS * (uint32_t)1000;
	///

	///Link Status.
	bool LinkActive = false;

	///Duplex.
	bool EvenSlot = false;
	//Helper.
	uint32_t DuplexElapsed;
	///

	///Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;
	///

	///Synced clock
	ClockSource SyncedClock;
	///

	///Crypto
	LoLaCryptoEncoder	CryptoEncoder;
	///

	///For use of estimated latency features
	uint32_t ETTM = 0;//Estimated transmission time in microseconds.
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
	ILoLaDriver() : PacketMap(), SyncedClock()
	{
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

#ifdef LOLA_MOCK_PACKET_LOSS
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

	uint32_t GetETTMMicros()
	{
		return ETTM;
	}

	void SetETTM(const uint32_t ettmMicros)
	{
#ifdef LOLA_LINK_USE_LATENCY_COMPENSATION
		ETTM = ettmMicros;
#else
		ETTM = LOLA_LINK_DEFAULT_LATENCY_COMPENSATION_MILLIS * (uint32_t)1000;
#endif
	}

	void SetDuplexSlot(const bool evenSlot)
	{
		EvenSlot = evenSlot;
	}

	void ResetStatistics()
	{
		TransmitedCount = 0;
		ReceivedCount = 0;
		RejectedCount = 0;
		TimingCollisionCount = 0;
	}

	void ResetLiveData()
	{
		ETTM = 0;

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

	uint64_t GetReceivedCount()
	{
		return ReceivedCount;
	}

	uint64_t GetRejectedCount()
	{
		return RejectedCount;
	}

	uint64_t GetTransmitedCount()
	{
		return TransmitedCount;
	}

	uint64_t GetTimingCollisionCount()
	{
		return TimingCollisionCount;
	}

	int16_t GetLastRSSI()
	{
		return LastReceivedInfo.RSSI;
	}

	uint32_t GetElapsedMillisLastValidReceived()
	{
		if (LastValidReceivedInfo.Micros == ILOLA_INVALID_MICROS)
		{
			return ILOLA_INVALID_MICROS;
		}
		else
		{
			return (micros() - LastValidReceivedInfo.Micros) / 1000;
		}
	}

	uint32_t GetElapsedMillisLastValidSent()
	{
		if (LastValidSentInfo.Micros == ILOLA_INVALID_MICROS)
		{
			return ILOLA_INVALID_MICROS;
		}
		else
		{
			return (micros() - LastValidSentInfo.Micros) / 1000;
		}
	}

	uint32_t GetLastValidReceivedMicros()
	{
		return LastValidReceivedInfo.Micros;
	}

	uint32_t GetLastValidSentMicros()
	{
		return LastValidSentInfo.Micros;
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
	virtual bool AllowedSend() { return false; }
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