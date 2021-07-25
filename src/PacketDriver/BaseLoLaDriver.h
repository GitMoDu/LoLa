// BaseLoLaDriver.h

#ifndef _BASE_LOLA_DRIVER_h
#define _BASE_LOLA_DRIVER_h

#include <stdint.h>
#include <LoLaDefinitions.h>

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Packet\LoLaAckNotifier.h>

#include <LoLaClock\TickTrainedTimerSyncedClock.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <LoLaClock\LoLaClock.h>
#include <LoLaClock\ClockTicker.h>
#include <LoLaDefinitions.h>

#include <PacketDriver\LoLaPacketDriverSettings.h>
#include <PacketDriver\PacketIOInfo.h>
#include <Link\LoLaLinkInfo.h>

class BaseLoLaDriver
{
public:
	// Synced clock source.
	ClockTicker SyncedClock;

	// Crypto encoder.
	LoLaCryptoEncoder CryptoEncoder;

	// LoLaLinkInfo
	PacketIOInfo IOInfo;

	// LoLaLinkInfo
	LoLaLinkInfo LinkInfo;

	// Packet Mapper for known definitions.
	LoLaPacketMap PacketMap;


	LoLaPacketDriverSettings* DriverSettings = nullptr;

protected:

	// Duplex.
	bool EvenSlot = false;

	LoLaAckNotifier AckNotifier;

	NotifiableIncomingPacket IncomingPacket;
	OutgoingIdPacket OutgoingPacket;
	//uint8_t* OutgoingPacketPayload;

	PacketDefinition* AckDefinition = nullptr;

	enum DriverActiveState
	{
		DriverDisabled,
		ReadyReceiving,
		BlockedForIncoming,
		SendingOutgoing
	};

	volatile DriverActiveState DriverState = DriverActiveState::DriverDisabled;


protected:
	// Timings.
	static const uint32_t DuplexPeriodMicros = ILOLA_DUPLEX_PERIOD_MILLIS * (uint32_t)1000;
	static const uint32_t HalfDuplexPeriodMicros = DuplexPeriodMicros / 2;
	static const uint32_t BackOffPeriodUnlinkedMillis = LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS;


public:
	// Packet Driver implementation.
	// Used by channel and power manager.
	virtual void InvalidateChannel() {}
	virtual void InvalidatedTransmitPower() {}

public:
	BaseLoLaDriver(LoLaPacketDriverSettings* settings)
		: SyncedClock()
		, CryptoEncoder(&SyncedClock)
		, IOInfo(settings)
		, LinkInfo(&IOInfo, &SyncedClock)
		, PacketMap()
		, AckNotifier(&PacketMap)
		, DriverSettings(settings)
		, IncomingPacket()
		, OutgoingPacket()
	{
		AckDefinition = PacketMap.GetAckDefinition();

	}

	virtual bool Setup()
	{
		return AckDefinition != nullptr
			&& SyncedClock.Setup()
			&& CryptoEncoder.Setup();
	}

	LoLaLinkIndicator* GetLinkIndicator()
	{
		return (LoLaLinkIndicator*)&LinkInfo;
	}

	void SetDuplexSlot(const bool evenSlot)
	{
		EvenSlot = evenSlot;
	}

	bool IsReady()
	{
		return DriverState == DriverActiveState::ReadyReceiving;
	}

	void NotifyServicesLinkStatusChanged()
	{
		const bool hasLink = LinkInfo.HasLink();

		ILoLaService* NotifyListener = nullptr;

		const uint8_t count = PacketMap.GetMaxIndex();

		for (uint8_t i = 0 + 1; i < count; i++)
		{
			// Skip the Ack definition.
			NotifyListener = PacketMap.GetServiceNotAckAt(i);
			if (NotifyListener != nullptr)
			{
				NotifyListener->OnLinkStatusChanged(hasLink);
			}
		}
	}

	const bool AllowedSend()
	{
		if (DriverState != DriverActiveState::ReadyReceiving)
		{
			//Serial.print(F("Nein State: "));
			//Serial.println(DriverState);
			return false;
		}
		else if (LinkInfo.HasLink())
		{
			return IsInSendSlot();
		}
		else
		{
			if (IOInfo.GetElapsedMillisLastValidSent() >= BackOffPeriodUnlinkedMillis) {

				return true;
			}

			return false;
		}
	}

private:
	const bool IsInSendSlot()
	{
		const uint32_t DuplexElapsed = (SyncedClock.GetSyncMicros() + DriverSettings->ETTM) % DuplexPeriodMicros;

		// Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed < HalfDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if (DuplexElapsed >= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}

#ifdef DEBUG_LOLA
public:
	virtual void Debug(Stream* serial)
	{
		PacketMap.Debug(serial);

		serial->println(F("Radio constants:"));

		serial->print(F(" TransmitPowerMin: "));
		serial->println(DriverSettings->TransmitPowerMin);
		serial->print(F(" TransmitPowerMax: "));
		serial->println(DriverSettings->TransmitPowerMax);

		serial->print(F(" RSSIMin: "));
		serial->println(DriverSettings->RSSIMin);
		serial->print(F(" RSSIMax: "));
		serial->println(DriverSettings->RSSIMax);

		serial->print(F(" ChannelMin: "));
		serial->println(DriverSettings->ChannelMin);
		serial->print(F(" ChannelMax: "));
		serial->println(DriverSettings->ChannelMax);
	}

	void CheckReceiveCollision(const int32_t offsetMicros)
	{
		if (LinkInfo.HasLink() &&
			(!IsInReceiveSlot(offsetMicros)))
		{
			IOInfo.TimingCollisionCount++;
		}
	}

	const bool IsInReceiveSlot(const int32_t offsetMicros)
	{
		const uint32_t DuplexElapsed = (uint32_t)(SyncedClock.GetSyncMicros() + offsetMicros) % DuplexPeriodMicros;

		// Even spread of true and false across the DuplexPeriod.
		if (EvenSlot)
		{
			if (DuplexElapsed >= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}
		else
		{
			if (DuplexElapsed <= HalfDuplexPeriodMicros)
			{
				return true;
			}
		}

		return false;
	}
#endif
};
#endif