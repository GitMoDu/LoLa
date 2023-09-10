// AbstractLoLaLinkPacket.h

#ifndef _ABSTRACT_LOLA_LINK_PACKET_
#define _ABSTRACT_LOLA_LINK_PACKET_


#include "AbstractLoLaReceiver.h"

#include "..\..\Link\IChannelHop.h"

#include "..\..\Link\IDuplex.h"
#include "..\..\Link\LoLaLinkSession.h"


/*
* https://github.com/FrankBoesing/FastCRC
*/
#include <FastCRC.h>



/// <summary>
/// Implements PacketServce functionality and leaves interfaces open for implementation.
/// Implements channel management.
/// As a partial abstract class, it implements the following ILoLaLink calls:
///		- CanSendPacket.
///		- GetRxChannel.
/// </summary>
class AbstractLoLaLinkPacket
	: public AbstractLoLaReceiver
	, public virtual IChannelHop::IHopListener
{
private:
	using BaseClass = AbstractLoLaReceiver;

	static constexpr uint_least16_t CALIBRATION_ROUNDS = F_CPU / 2000000L;

protected:
	/// <summary>
	/// HKDF Expanded key, with extra seeds.
	/// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey;

	LoLaRandom RandomSource; // Cryptographic Secure(ish) Random Number Generator.

	FastCRC8 FastHasher; // 8 bit fast hasher for deterministic PRNG.

protected:
	// Collision avoidance time slotting.
	IDuplex* Duplex;

	/// <summary>
	/// Channel Hopper calls back when it is time to update the RX channel, if it's not up to date.
	/// </summary>
	IChannelHop* ChannelHopper;

	// Sync clock helper.
	Timestamp LinkTimestamp{};

private:
	uint32_t StageStartTime = 0;

	const bool IsLinkHopper;

public:
	AbstractLoLaLinkPacket(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: IChannelHop::IHopListener()
		, BaseClass(scheduler, linkRegistry, encoder, transceiver, cycles)
		, ExpandedKey()
		, RandomSource(entropy)
		, FastHasher()
		, Duplex(duplex)
		, ChannelHopper(hop)
		, LinkTimestamp()
		, IsLinkHopper(hop->GetHopPeriod() == IChannelHop::NOT_A_HOPPER)
	{}

#if defined(DEBUG_LOLA)
	virtual void DebugClock() final
	{
		//SyncClock.Debug();
	}
#endif

	virtual const bool Setup()
	{
		if (!RandomSource.Setup())
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("RandomSource setup failed."));
#endif
			return false;
		}

		if (ChannelHopper == nullptr ||
			!ChannelHopper->Setup(this, &SyncClock))
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("ChannelHopper setup failed."));
#endif
			return false;
		}

		if (!CalibrateSendDuration())
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("CalibrateSendDuration failed."));
#endif
			return false;
		}

		if (Duplex->GetPeriod() != IDuplex::DUPLEX_FULL && GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE) >= Duplex->GetRange())
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("Estimated Time-On-Air is longer than the duplex slot duration."));
#endif
			return false;
		}

		if (Duplex->GetPeriod() != IDuplex::DUPLEX_FULL && Duplex->GetPeriod() > LoLaLinkDefinition::DUPLEX_PERIOD_MAX_MICROS)
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("Duplex Period is too long for LoLa Link."));
#endif
			return false;
		}

#if defined(DEBUG_LOLA)
		Serial.print(F("Duplex Range: "));
		Serial.println(Duplex->GetRange());

		Serial.print(F("Duplex Period: "));
		Serial.println(Duplex->GetPeriod());

		Serial.print(F("Max Time-On-Air: "));
		Serial.println(GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
		Serial.println();
#endif

		return BaseClass::Setup();
	}

	/// <summary>
	/// IChannelHopper overrides.
	/// </summary>
public:
	virtual void OnChannelHopTime() final
	{
		PacketService.RefreshChannel();
	}

	virtual const bool HasLink() final
	{
		return LinkStage == LinkStageEnum::Linked;
	}

	/// <summary>
	/// ILoLaLink overrides.
	/// </summary>
public:
	virtual const uint32_t GetPacketThrottlePeriod() final
	{
		if (Duplex->GetPeriod() == IDuplex::DUPLEX_FULL)
		{
			return Transceiver->GetTimeToAir(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
				+ Transceiver->GetDurationInAir(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
				+ LoLaLinkDefinition::FULL_DUPLEX_RESEND_WAIT_MICROS;
		}
		else
		{
			return Duplex->GetPeriod() / 2;
		}
	}

	virtual const bool CanSendPacket(const uint8_t payloadSize) final
	{
		if (LinkStage == LinkStageEnum::Linked && PacketService.CanSendPacket())
		{
			if (Duplex->GetPeriod() == IDuplex::DUPLEX_FULL)
			{
				return true;
			}
			else
			{
				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

				return Duplex->IsInRange(LinkTimestamp.GetRollingMicros(), GetOnAirDuration(payloadSize));
			}
		}
		else
		{
			return false;
		}
	}

	/// <summary>
	/// IPacketServiceListener overrides.
	/// </summary>
public:
	virtual const uint8_t GetRxChannel() final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linking:
			return ChannelHopper->GetFixedChannel();
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return GetPrngHopChannel(ChannelHopper->GetTimedHopIndex());
			}
			else
			{
				return ChannelHopper->GetFixedChannel();
			}
		default:
			break;
		}

		return 0;
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		if (linkStage != LinkStage)
		{
			switch (LinkStage)
			{
			case LinkStageEnum::Linked:
				ChannelHopper->OnLinkStopped();
				Registry->NotifyLinkListeners(false);
				break;
			default:
				break;
			}

			LinkStage = linkStage;
			StageStartTime = millis();

			switch (linkStage)
			{
			case LinkStageEnum::Disabled:
				Task::disable();
				break;
			case LinkStageEnum::AwaitingLink:
				SendCounter = RandomSource.GetRandomShort();
				PacketService.RefreshChannel();
				break;
			case LinkStageEnum::Linking:
				Task::enable();
				break;
			case LinkStageEnum::Linked:
				ChannelHopper->OnLinkStarted();
				Registry->NotifyLinkListeners(true);
				Task::enable();
				break;
			default:
				Task::enable();
				break;
			}
		}
	}

protected:
	///// <summary>
	///// During Link, if the configured channel management is a hopper,
	///// every N us the channel switches.
	///// </summary>
	///// <param name="rollingMicros">[0;UINT32_MAX]</param>
	///// <returns>Normalized Tx Chanel.</returns>
	virtual const uint8_t GetTxChannel(const uint32_t rollingMicros) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linking:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return GetPrngHopChannel(ChannelHopper->GetHopIndex(rollingMicros));
			}
			else
			{
				return ChannelHopper->GetFixedChannel();
			}
		default:
			break;
		}

		return 0;
	}

protected:
	const uint32_t GetStageElapsedMillis()
	{
		return millis() - StageStartTime;
	}

	void ResetStageStartTime()
	{
		StageStartTime = millis();
	}

	void SetHopperFixedChannel(const uint8_t channel)
	{
		ChannelHopper->SetFixedChannel(channel);
		PacketService.RefreshChannel();
	}

private:
	const uint8_t GetPrngHopChannel(const uint32_t tokenIndex)
	{
		// Start Hash with Token Seed.
		FastHasher.smbus(ExpandedKey.ChannelSeed, LoLaLinkDefinition::CHANNEL_KEY_SIZE);

		// Add Token Index and return the mod of the result.
		return FastHasher.smbus_upd((uint8_t*)&tokenIndex, sizeof(uint32_t));
	}

	/// <summary>
	/// Calibrate CPU dependent durations.
	/// </summary>
	const bool CalibrateSendDuration()
	{
		const uint32_t calibrationStart = micros();

		// Measure short (baseline) packet first.
		uint32_t start = 0;
		uint32_t shortDuration = 0;
		uint32_t longDuration = 0;

		OutPacket.SetPort(123);
		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAX_PAYLOAD_SIZE; i++)
		{
			OutPacket.Payload[i] = i + 1;
		}

		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (!BaseClass::MockSendPacket(OutPacket.Data, 0))
			{
				// Calibration failed.
#if defined(DEBUG_LOLA)
				Serial.print(F("Calibration failed on short test."));
#endif
				return false;
			}
		}
		shortDuration = micros();
		shortDuration -= start;
		shortDuration /= CALIBRATION_ROUNDS;


		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (!BaseClass::MockSendPacket(OutPacket.Data, LoLaPacketDefinition::MAX_PAYLOAD_SIZE))
			{
				// Calibration failed.
#if defined(DEBUG_LOLA)
				Serial.print(F("Calibration failed on long test."));
#endif
				return false;
			}
		}
		longDuration = micros();
		longDuration -= start;
		longDuration /= CALIBRATION_ROUNDS;

		if (longDuration <= shortDuration)
		{
			longDuration = shortDuration + 1;
		}

		const uint32_t calibrationDuration = micros() - calibrationStart;

#if defined(DEBUG_LOLA)
		Serial.print(F("Calibration done. ("));
		Serial.print(CALIBRATION_ROUNDS);
		Serial.print(F(" rounds took "));
		Serial.print(calibrationDuration);
		Serial.println(F(" us)"));
		Serial.println();
#endif

		return SetSendCalibration(shortDuration, longDuration);
	}
};
#endif