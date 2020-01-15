// ILoLaDriver.h

#ifndef _ILOLA_h
#define _ILOLA_h

#include <stdint.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <LoLaClock\IClock.h>
#include <LoLaDefinitions.h>

#include <PacketDriver\ILoLaSelector.h>
#include <PacketDriver\LinkIndicator.h>



class ILoLaDriver
{
private:
	class InputInfoType
	{
	public:
		uint32_t Micros = 0;
		int16_t RSSI = ILOLA_INVALID_RSSI;

		void Clear()
		{
			Micros = 0;
			RSSI = ILOLA_INVALID_RSSI;
		}
	};

	class OutputInfoType
	{
	public:
		uint32_t Micros = 0;
		uint8_t Id = 0;

		void Clear()
		{
			Micros = 0;
		}
	};

protected:
	// Global statistics.
	uint64_t TransmitedCount = 0;
	uint64_t ReceivedCount = 0;
	uint64_t RejectedCount = 0;
	uint64_t StateCollisionCount = 0;
#ifdef DEBUG_LOLA
	uint64_t TimingCollisionCount = 0;
#endif

	// Incoming packet.
	InputInfoType LastReceivedInfo;

	// Outgoing packet.
	InputInfoType LastValidReceivedInfo;
	OutputInfoType LastValidSentInfo;

	/// External sources. ///
	// Transmit power selector.
	ITransmitPowerSelector* TransmitPowerSelector = nullptr;

	// Channel selector.
	IChannelSelector* ChannelSelector = nullptr;

	// Synced clock source.
	ISyncedClock* SyncedClock = nullptr;

	// Link Status source.
	LinkIndicator* LinkStatusIndicator = nullptr;

	// Crypto encoder.
	LoLaCryptoEncoder* CryptoEncoder = nullptr;
	///

	// Duplex.
	bool EvenSlot = false;

	// Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;

	///For use of estimated latency features
	uint32_t ETTM = 0;//Estimated transmission time in microseconds.
	///

	enum DriverActiveStates :uint8_t
	{
		DriverDisabled,
		ReadyForAnything,
		BlockedForIncoming,
		SendingOutgoing
	};

public:
	ILoLaDriver() : PacketMap()
	{

	}

	ISyncedClock* GetSyncedClock()
	{
		return SyncedClock;
	}

	void SetSyncedClock(ISyncedClock* syncedClock)
	{
		SyncedClock = syncedClock;
	}

	LoLaCryptoEncoder* GetCryptoEncoder()
	{
		return CryptoEncoder;
	}

	void SetCryptoEncoder(LoLaCryptoEncoder* cryptoEncoder)
	{
		CryptoEncoder = cryptoEncoder;
	}

	LinkIndicator* GetLinkStatusIndicator()
	{
		return LinkStatusIndicator;
	}

	void SetLinkStatusIndicator(LinkIndicator* linkStatusIndicator)
	{
		if (linkStatusIndicator != nullptr)
		{
			LinkStatusIndicator = linkStatusIndicator;
		}
	}

	void SetChannelSelector(IChannelSelector* channelSelector)
	{
		if (channelSelector != nullptr)
		{
			ChannelSelector = channelSelector;
		}
	}

	void SetTransmitPowerSelector(ITransmitPowerSelector* transmitPowerSelector)
	{
		if (transmitPowerSelector != nullptr)
		{
			TransmitPowerSelector = transmitPowerSelector;
		}
	}

	bool HasLink()
	{
		return LinkStatusIndicator->HasLink();
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

	void SetDuplexSlot(const bool evenSlot)
	{
		EvenSlot = evenSlot;
	}

	void ResetStatistics()
	{
		TransmitedCount = 0;
		ReceivedCount = 0;
		RejectedCount = 0;
		StateCollisionCount = 0;
#ifdef DEBUG_LOLA
		TimingCollisionCount = 0;
#endif
	}

	void ResetLiveData()
	{
		ETTM = 0;

		LastReceivedInfo.Clear();

		LastValidReceivedInfo.Clear();
		LastValidSentInfo.Clear();

		ResetStatistics();
		OnResetLiveData();
	}

	void Stop()
	{
		OnStop();
	}

	void Start()
	{
		ResetLiveData();
		if (Validate())
		{
			OnStart();
		}
#ifdef DEBUG_LOLA
		else
		{
			Serial.println(F("[ERROR] LoLa Driver needs a synced clock."));
		}
#endif // DEBUG_LOLA		
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

#ifdef DEBUG_LOLA
	uint64_t GetTimingCollisionCount()
	{
		return TimingCollisionCount;
	}
#endif

	uint64_t GetStateCollisionCount()
	{
		return StateCollisionCount;
	}

	int16_t GetLastRSSI()
	{
		return LastValidReceivedInfo.RSSI;
	}

	uint32_t GetElapsedMillisLastValidReceived()
	{
		if (LastValidReceivedInfo.Micros == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return (micros() - LastValidReceivedInfo.Micros) / 1000;
		}
	}

	uint32_t GetElapsedMillisLastValidSent()
	{
		if (LastValidSentInfo.Micros == 0)
		{
			return UINT32_MAX;
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

	uint8_t GetTransmitPowerRange()
	{
		return GetTransmitPowerMax() - GetTransmitPowerMin();
	}

	uint8_t GetTransmitPower()
	{
		return GetTransmitPowerMin() + (uint8_t)(((uint16_t)TransmitPowerSelector->GetTransmitPowerNormalized() * GetTransmitPowerRange()) / UINT8_MAX);
	}

	uint8_t GetTransmitPowerNormalized()
	{
		return TransmitPowerSelector->GetTransmitPowerNormalized();
	}

	inline uint8_t GetChannel()
	{
		return ChannelSelector->GetChannel();
	}

	ILoLaPacketMap* GetPacketMap()
	{
		return &PacketMap;
	}

public:
	// Driver constants' overload.
	virtual const uint8_t GetChannelMax() { return 0; }
	virtual const uint8_t GetChannelMin() { return 0; }
	virtual const uint8_t GetMaxPacketSize() { return 0; }

	// Driver constants' overload.
	virtual const uint8_t GetTransmitPowerMax() { return 1; }
	virtual const uint8_t GetTransmitPowerMin() { return 0; }

	virtual const int16_t GetRSSIMax() { return 0; }
	virtual const int16_t GetRSSIMin() { return -50; }


protected:
	virtual bool Validate()
	{
		return SyncedClock != nullptr &&
			CryptoEncoder != nullptr &&
			LinkStatusIndicator != nullptr &&
			TransmitPowerSelector != nullptr &&
			ChannelSelector != nullptr;
	}

	// Oportune overload to do some houseleeping, if needed.
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void OnResetLiveData() {}

public:
	//Packet driver implementation.
	virtual bool SendPacket(ILoLaPacket* packet) { return false; }
	
	virtual bool AllowedSend() { return false; }

	virtual void OnChannelUpdated() {}
	virtual void OnTransmitPowerUpdated() {}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);

		serial->println(F("Radio constants:"));

		serial->print(F(" TransmitPowerMin: "));
		serial->println(GetTransmitPowerMin());
		serial->print(F(" TransmitPowerMax: "));
		serial->println(GetTransmitPowerMax());

		serial->print(F(" RSSIMin: "));
		serial->println(GetRSSIMin());
		serial->print(F(" RSSIMax: "));
		serial->println(GetRSSIMax());

		serial->print(F(" ChannelMin: "));
		serial->println(GetChannelMin());
		serial->print(F(" ChannelMax: "));
		serial->println(GetChannelMax());
	}
#endif
};
#endif