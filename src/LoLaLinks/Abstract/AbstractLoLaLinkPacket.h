// AbstractLoLaLinkPacket.h

#ifndef _ABSTRACT_LOLA_LINK_PACKET_
#define _ABSTRACT_LOLA_LINK_PACKET_

#include <IDuplex.h>
#include <IChannelHop.h>

#include "AbstractLoLaReceiver.h"
#include "..\..\Link\LoLaLinkSession.h"

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

#if defined(F_CPU)
	static constexpr uint32_t CPU_CLOCK = F_CPU;
#elif defined(CLOCK_SPEED_HZ)
	static constexpr uint32_t CPU_CLOCK = CLOCK_SPEED_HZ;
#endif

	/// <summary>
	/// Faster MCUs need more rounds for an accurate calibration.
	/// </summary>
	static constexpr uint16_t CALIBRATION_ROUNDS = CPU_CLOCK / 1500000L;

	/// <summary>
	/// Slower MCUs need a bigger look ahead to counter scheduler latency.
	/// </summary>
	static constexpr uint16_t HOPPER_OFFSET = 500000000 / CPU_CLOCK;

protected:
	/// <summary>
	/// Cryptographic Secure(ish) Random Number Generator.
	/// </summary>
	LoLaRandom RandomSource;

	/// <summary>
	/// Collision avoidance time slotting.
	/// </summary>
	IDuplex* Duplex;

	/// <summary>
	/// Channel Hopper calls back when it is time to update the RX channel, if it's not up to date.
	/// </summary>
	IChannelHop* ChannelHopper;

	/// <summary>
	/// Sync clock helper.
	/// </summary>
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
		, RandomSource(entropy)
		, Duplex(duplex)
		, ChannelHopper(hop)
		, LinkTimestamp()
		, IsLinkHopper(hop->GetHopPeriod() != IChannelHop::NOT_A_HOPPER)
	{}

public:
	virtual const bool Setup()
	{
		if (!BaseClass::Setup())
		{
			return false;
		}

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

		if (GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE) >= Duplex->GetRange())
		{
#if defined(DEBUG_LOLA)
			Serial.print(F("Estimated Time-On-Air ( is longer than the duplex slot duration."));
			Serial.print(GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
			Serial.print(F("us) is longer than the duplex slot duration ("));
			Serial.print(Duplex->GetRange());
			Serial.println(F(" us)."));
#endif
			return false;
		}

		if (Duplex->GetPeriod() > LoLaLinkDefinition::DUPLEX_PERIOD_MAX_MICROS)
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("Duplex Period is too long for LoLa Link."));
#endif
			return false;
		}

#if defined(DEBUG_LOLA)
		const uint32_t transceiverBitsPerSecond = ((uint32_t)LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE * 8 * ONE_SECOND_MICROS)
			/ GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE);
		const uint32_t packetsPerSecond = ONE_SECOND_MICROS / (GetSendDuration(0) + GetOnAirDuration(0));
		const uint32_t rangedPacketsPerSecond = (packetsPerSecond * Duplex->GetRange()) / Duplex->GetPeriod();
		const uint32_t rangedBitsPerSecond = (transceiverBitsPerSecond * Duplex->GetRange()) / Duplex->GetPeriod();

		Serial.println(F("Time-to-Air estimation:"));
		Serial.print(F("\tShort\t"));
		Serial.print(GetSendDuration(0));
		Serial.println(F(" us"));
		Serial.print(F("\tLong\t"));
		Serial.print(GetSendDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
		Serial.println(F(" us"));

		Serial.println(F("Time-On-Air:"));
		Serial.print(F("\tMin:\t"));
		Serial.print(GetOnAirDuration(0));
		Serial.println(F(" us."));
		Serial.print(F("\tMax:\t"));
		Serial.print(GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
		Serial.println(F(" us."));

		Serial.println(F("Packet TX"));
		Serial.print(F("\tMax:\t"));
		Serial.print(packetsPerSecond);
		Serial.println(F(" packets/s"));
		Serial.print(F("\tRanged:\t"));
		Serial.print(rangedPacketsPerSecond);
		Serial.println(F(" packets/s"));

		Serial.println(F("Bitrate"));
		Serial.print(F("\tMax:\t"));
		Serial.print(transceiverBitsPerSecond);
		Serial.println(F(" bps"));

		Serial.print(F("\tRanged:\t"));
		Serial.print(rangedBitsPerSecond);
		Serial.println(F(" bps"));

		Serial.println(F("Duplex:"));
		Serial.print(F("\tPeriod:\t"));
		Serial.println(Duplex->GetPeriod());
		Serial.print(F("\tRange:\t"));
		Serial.println(Duplex->GetRange());

		Serial.println(F("Timeout Durations: "));
		Serial.print(F("\tLinking:\t"));
		Serial.print(GetLinkingTimeoutDuration());
		Serial.println(F(" us."));
		Serial.print(F("\tLink:\t\t"));
		Serial.print(GetLinkTimeoutDuration());
		Serial.println(F(" us."));
		Serial.print(F("\tTransition:\t"));
		Serial.print(LoLaLinkDefinition::GetTransitionDuration(Duplex->GetPeriod()));
		Serial.println(F(" us."));

		Serial.println();
#endif

		Encoder->GenerateProtocolId(
			Duplex->GetPeriod(),
			ChannelHopper->GetHopPeriod(),
			Transceiver->GetTransceiverCode());

		return true;
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
		return (((uint32_t)Duplex->GetPeriod()) * 2) - 1;
	}

	virtual const bool CanSendPacket(const uint8_t payloadSize) final
	{
		if (LinkStage == LinkStageEnum::Linked
			&& PacketService.CanSendPacket())
		{
			SyncClock.GetTimestamp(LinkTimestamp);
			LinkTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

			return Duplex->IsInRange(LinkTimestamp.GetRollingMicros(), GetOnAirDuration(payloadSize));
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
		case LinkStageEnum::Linking:
			return LoLaLinkDefinition::GetAdvertisingChannel(ChannelHopper->GetChannel());
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return Encoder->GetPrngHopChannel(ChannelHopper->GetTimedHopIndex());
			}
			else
			{
				return ChannelHopper->GetChannel();
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
				ChannelHopper->OnLinkStopped();
				SetSendCounter((uint16_t)RandomSource.GetRandomLong());
				SetReceiveCounter((uint16_t)RandomSource.GetRandomLong());
				PacketService.RefreshChannel();
				break;
			case LinkStageEnum::Linking:
				Task::enable();
				break;
			case LinkStageEnum::Linked:
				// Reset rolling counters.
				ReceivedCounter = 0;
				SentCounter = 0;

				// Set startup channel and start hopper.
				ChannelHopper->SetChannel(GetRxChannel());
				ChannelHopper->OnLinkStarted();
				PacketService.RefreshChannel();

				// Notify services that link is ready.
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
	virtual const uint8_t MockGetTxChannel(const uint32_t rollingMicros) final
	{
		if (IsLinkHopper)
		{
			return Encoder->GetPrngHopChannel(ChannelHopper->GetHopIndex(rollingMicros));
		}
		else
		{
			return ChannelHopper->GetChannel();
		}
	}

	/// <summary>
	/// During Link, if the configured channel management is a hopper,
	/// every N us the channel switches.
	/// </summary>
	/// <param name="rollingMicros">[0;UINT32_MAX]</param>
	/// <returns>Normalized Tx Chanel.</returns>
	virtual const uint8_t GetTxChannel(const uint32_t rollingMicros) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
		case LinkStageEnum::Linking:
			return LoLaLinkDefinition::GetAdvertisingChannel(ChannelHopper->GetChannel());
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return Encoder->GetPrngHopChannel(ChannelHopper->GetHopIndex(rollingMicros));
			}
			else
			{
				return ChannelHopper->GetChannel();
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

	void SetAdvertisingChannel(const uint8_t channel)
	{
		ChannelHopper->SetChannel(channel);
		PacketService.RefreshChannel();
	}

	const uint32_t GetLinkingTimeoutDuration()
	{
		return LoLaLinkDefinition::GetLinkingTimeoutDuration(Duplex->GetPeriod());
	}

	const uint32_t GetLinkTimeoutDuration()
	{
		return LoLaLinkDefinition::GetLinkTimeoutDuration(Duplex->GetPeriod());
	}

private:
	/// <summary>
	/// Calibrate CPU dependent durations.
	/// </summary>
	const bool CalibrateSendDuration()
	{
		uint32_t start = 0;
		OutPacket.SetPort(123);
		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAX_PAYLOAD_SIZE; i++)
		{
			OutPacket.Payload[i] = i + 1;
		}

		LOLA_RTOS_PAUSE();
#if defined(DEBUG_LOLA)
		const uint32_t calibrationStart = micros();
#endif
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (!BaseClass::MockSendPacket(OutPacket.Data, 0))
			{
				// Calibration failed.
				LOLA_RTOS_RESUME();
#if defined(DEBUG_LOLA)
				Serial.println(F("Calibration failed on short test."));
#endif
				return false;
			}
		}
		const uint32_t shortDuration = (micros() - start) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();
		LOLA_RTOS_PAUSE();
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (!BaseClass::MockSendPacket(OutPacket.Data, LoLaPacketDefinition::MAX_PAYLOAD_SIZE))
			{
				// Calibration failed.
				LOLA_RTOS_RESUME();
#if defined(DEBUG_LOLA)
				Serial.print(F("Calibration failed on long test."));
#endif
				return false;
			}
		}
		uint32_t longDuration = (micros() - start) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();
		if (longDuration <= shortDuration)
		{
			longDuration = shortDuration + 1;
		}

		LOLA_RTOS_PAUSE();
		// Measure Timestamping time.
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			SyncClock.GetTimestamp(LinkTimestamp);
		}
		const uint32_t timestampingDuration = (((uint64_t)(micros() - start)) * 1000) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();

		LOLA_RTOS_PAUSE();
		// Measure PRNG Hop calculation time.
		volatile uint8_t dummy = 0;
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			dummy += Encoder->GetPrngHopChannel(LinkTimestamp.GetRollingMicros() + i);
		}
		const uint32_t calculationDuration = (((uint64_t)(micros() - start)) * 1000) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();
		const uint32_t prngHopDuration = (timestampingDuration + calculationDuration) / 1000;

		if (prngHopDuration > (UINT16_MAX - HOPPER_OFFSET))
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("Hopper duration too long: "));
			Serial.print(prngHopDuration);
			Serial.println(F(" us."));
#endif
			return false;
		}

#if defined(DEBUG_LOLA)
		LOLA_RTOS_PAUSE();
		const uint32_t calibrationDuration = micros() - calibrationStart;

		// Measure rejection time.
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (BaseClass::MockReceiveFailPacket(micros(), OutPacket.Data, 0))
			{
				// Calibration failed.
				LOLA_RTOS_RESUME();
				return false;
			}
		}
		const uint32_t rejectionShortDuration = (micros() - start) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();
		LOLA_RTOS_PAUSE();
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (BaseClass::MockReceiveFailPacket(micros(), OutPacket.Data, LoLaPacketDefinition::MAX_PAYLOAD_SIZE))
			{
				// Calibration failed.
				Serial.print(F("Decrypt succeeded. It shouldn\'t."));
				LOLA_RTOS_RESUME();
				return false;
			}
		}
		const uint32_t rejectionLongDuration = (micros() - start) / CALIBRATION_ROUNDS;
		LOLA_RTOS_RESUME();

		Serial.print(F("Calibration done. ("));
		Serial.print(CALIBRATION_ROUNDS);
		Serial.print(F(" rounds took "));
		Serial.print(calibrationDuration);
		Serial.println(F(" us)"));

		Serial.println(F("Encode"));
		Serial.print(F("\tShort:\t"));
		Serial.print(shortDuration);
		Serial.println(F(" us"));
		Serial.print(F("\tLong\t"));
		Serial.print(longDuration);
		Serial.println(F(" us"));

		Serial.println(F("Rejection: "));
		Serial.print(F("\tShort:\t"));
		Serial.print(rejectionShortDuration);
		Serial.println(F(" us"));
		Serial.print(F("\tLong\t"));
		Serial.print(rejectionLongDuration);
		Serial.println(F(" us"));
		Serial.println(F("PRNG Hop:"));
		Serial.print('\t');
		Serial.print(calculationDuration);
		Serial.println(F(" ns."));
		Serial.println(F("Timestamping"));
		Serial.print('\t');
		Serial.print(timestampingDuration);
		Serial.println(F(" ns."));
		Serial.println(F("Hopper offset"));
		Serial.print('\t');
		Serial.print(HOPPER_OFFSET);
		Serial.println(F(" us."));
		Serial.println(F("Hopper forward"));
		Serial.print('\t');
		Serial.print(prngHopDuration + HOPPER_OFFSET);
		Serial.println(F(" us."));
#endif

		ChannelHopper->SetHopTimestampOffset(prngHopDuration + HOPPER_OFFSET);

		return SetSendCalibration(shortDuration, longDuration);
	}
};
#endif