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

	/// <summary>
	/// Faster MCUs need more rounds for an accurate calibration.
	/// </summary>
	static constexpr uint16_t CALIBRATION_ROUNDS = F_CPU / 1500000L;

	/// <summary>
	/// Slower MCUs need a bigger look ahead to counter scheduler latency.
	/// </summary>
	static constexpr uint16_t HOPPER_OFFSET = 1500000000 / F_CPU;

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

		if (Duplex->GetPeriod() != IDuplex::DUPLEX_FULL
			&& GetOnAirDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE) >= Duplex->GetRange())
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

		if (Duplex->GetPeriod() != IDuplex::DUPLEX_FULL
			&& Duplex->GetPeriod() > LoLaLinkDefinition::DUPLEX_PERIOD_MAX_MICROS)
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
				SendCounter = RandomSource.GetRandomLong();
				LastValidReceivedCounter = RandomSource.GetRandomShort();
				PacketService.RefreshChannel();
				break;
			case LinkStageEnum::Linking:
				Task::enable();
				break;
			case LinkStageEnum::Linked:
				ReceivedCounter = 0;

				// Set startup channel and start hopper.
				ChannelHopper->SetChannel(GetRxChannel());
				ChannelHopper->OnLinkStarted();

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

private:
	/// <summary>
	/// Calibrate CPU dependent durations.
	/// </summary>
	const bool CalibrateSendDuration()
	{
#if defined(DEBUG_LOLA)
		const uint32_t calibrationStart = micros();
#endif

		uint32_t start = 0;
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
				Serial.println(F("Calibration failed on short test."));
#endif
				return false;
			}
		}
		const uint32_t shortDuration = (micros() - start) / CALIBRATION_ROUNDS;

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
		uint32_t longDuration = (micros() - start) / CALIBRATION_ROUNDS;

		if (longDuration <= shortDuration)
		{
			longDuration = shortDuration + 1;
		}

		// Measure Timestamping time.
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			SyncClock.GetTimestampMonotonic(LinkTimestamp);
		}
		const uint32_t timestampingDuration = (((uint64_t)(micros() - start)) * 1000) / CALIBRATION_ROUNDS;

#if defined(DEBUG_LOLA)
		Serial.println(F("Timestamping duration: "));
		Serial.print(timestampingDuration);
		Serial.println(F(" ns."));
#endif

		// Measure PRNG Hop calculation time.
		volatile bool dummy = 0;
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			dummy ^= Encoder->GetPrngHopChannel(LinkTimestamp.GetRollingMicros() + i);
		}
		const uint32_t calculationDuration = (((uint64_t)(micros() - start)) * 1000) / CALIBRATION_ROUNDS;

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
		const uint32_t calibrationDuration = micros() - calibrationStart;
		Serial.print(F("Calibration done. ("));
		Serial.print(CALIBRATION_ROUNDS);
		Serial.print(F(" rounds took "));
		Serial.print(calibrationDuration);
		Serial.println(F(" us)"));

		Serial.println(F("Short\tLong"));
		Serial.print(shortDuration);
		Serial.print('\t');
		Serial.print(longDuration);
		Serial.println(F(" us"));

		// Measure rejection time.
		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (BaseClass::MockReceiveFailPacket(micros(), OutPacket.Data, 0))
			{
				// Calibration failed.
				Serial.print(F("Decrypt succeeded. It shouldn\'t."));
			}
		}
		const uint32_t rejectionShortDuration = (micros() - start) / CALIBRATION_ROUNDS;

		start = micros();
		for (uint_fast16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (BaseClass::MockReceiveFailPacket(micros(), OutPacket.Data, LoLaPacketDefinition::MAX_PAYLOAD_SIZE))
			{
				// Calibration failed.
				Serial.print(F("Decrypt succeeded. It shouldn\'t."));
			}
		}
		const uint32_t rejectionLongDuration = (micros() - start) / CALIBRATION_ROUNDS;

		Serial.println(F("Rejection duration: "));
		Serial.println(F("Short\tLong"));
		Serial.print(rejectionShortDuration);
		Serial.print('\t');
		Serial.print(rejectionLongDuration);
		Serial.println(F(" us"));
		Serial.println(F("PRNG Hop calculation: "));
		Serial.print(calculationDuration);
		Serial.println(F(" ns."));
		Serial.println(F("Hopper offset: "));
		Serial.print(HOPPER_OFFSET);
		Serial.println(F(" us."));
		Serial.println(F("Hopper look forward: "));
		Serial.print(prngHopDuration + HOPPER_OFFSET);
		Serial.println(F(" us."));
#endif

		ChannelHopper->SetHopTimestampOffset(prngHopDuration + HOPPER_OFFSET);

		return SetSendCalibration(shortDuration, longDuration);
	}
};
#endif