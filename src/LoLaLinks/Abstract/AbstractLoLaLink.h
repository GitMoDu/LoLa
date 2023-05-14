// AbstractLoLaLink.h
#ifndef _ABSTRACT_LOLA_LINK_
#define _ABSTRACT_LOLA_LINK_


#include "AbstractPublicKeyLoLaLink.h"
#include "..\..\Link\ReportTracker.h"
#include "..\..\Link\TimedStateTransition.h"


/// <summary>
/// 
/// </summary>
/// <typeparam name="MaxPayloadSize"></typeparam>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaLink : public AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;


	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;


protected:
	using BaseClass::Driver;
	using BaseClass::OutPacket;
	using BaseClass::Duplex;
	using BaseClass::LinkStage;
	using BaseClass::LastValidReceivedCounter;
	using BaseClass::SendCounter;
	using BaseClass::SyncClock;
	using BaseClass::LastReceivedRssi;

	using BaseClass::Session;

	using BaseClass::RegisterPort;
	using BaseClass::RegisterPacketReceiver;
	using BaseClass::RegisterPacketReceiverInternal;
	using BaseClass::GetSendDuration;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetLastValidReceived;
	using BaseClass::SetSendCalibration;

	using BaseClass::CheckPendingAck;

	using BaseClass::PostLinkState;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::GetElapsedSinceLastValidReceived;

private:
	static constexpr uint16_t CALIBRATION_ROUNDS = (F_CPU / (3333000L));

	/// <summary>
	/// Slow value, let the main services hog the CPU.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;


private:
	ReportTracker<LoLaLinkDefinition::REPORT_UPDATE_PERIOD,
		LoLaLinkDefinition::REPORT_RESEND_PERIOD,
		LoLaLinkDefinition::REPORT_PARTNER_SILENCE_TRIGGER_PERIOD> ReportTracking;

	uint32_t LinkStartSeconds = 0;

protected:
	/// <summary>
	/// </summary>
	/// <param name="stateElapsed">Millis elapsed since link state start.</param>
	virtual void OnLinking(const uint32_t stateElapsed) {}

	/// <summary>
	/// </summary>
	/// <param name="stateElapsed">Millis elapsed since link state start.</param>
	virtual void OnAwaitingLink(const uint32_t stateElapsed) {}

	/// <summary>
	/// </summary>
	/// <returns>True if a clock sync update is due or pending to send.</returns>
	virtual const bool CheckForClockSyncUpdate() { return false; }

public:
	AbstractLoLaLink(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, driver, entropySource, clockSource, timerSource, duplex, hop, publicKey, privateKey, accessPassword)
		, ReportTracking()
	{}

	virtual const bool Setup()
	{
		if (BaseClass::Setup()
			&& RegisterPacketReceiverInternal(this, Unlinked::PORT)
			&& RegisterPacketReceiverInternal(this, Linking::PORT)
			&& RegisterPort(Linked::PORT)
			&& CalibrateSendDuration())
		{
			return true;
		}

		return false;
	}

	virtual const uint32_t GetLinkDuration() final
	{
		return SyncClock.GetSeconds(0) - LinkStartSeconds;
	}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port)
	{
		if (port == Linked::PORT)
		{
			switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
			{
			case Linked::ReportUpdate::SUB_HEADER:
				if (payloadSize == Linked::ReportUpdate::PAYLOAD_SIZE)
				{
					// Keep track of partner's RSSI, for output gain management.
					ReportTracking.OnReportReceived(millis(), payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX]);

					// Update send counter, to prevent mismatches.
					if ((SendCounter - payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX]) > LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE)
					{
						// Recover send counter to report provided.
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Report recovered counter delta: "));
						Serial.println(SendCounter - payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] - LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE);
#endif
						SendCounter = payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] + LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE + 1;
					}

					// Check if partner is requesting a report back.
					if (payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] > 0)
					{
						RequestReportUpdate();
					}
				}
				break;
			default:
				// This is the top-most class to parse incoming packets as a consumer.
				// So the base class call is not needed here.
				break;
			}
		}
	}

	virtual const bool Start() final
	{
		if (LinkStage == LinkStageEnum::Disabled)
		{
			UpdateLinkStage(LinkStageEnum::Booting);

			return true;
		}
		else
		{
			Task::enableIfNot();

			return false;
		}
	}

	virtual const bool Stop() final
	{
		UpdateLinkStage(LinkStageEnum::Disabled);

		return Driver->DriverStop();
	}

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() {}
#endif

protected:
	/// <summary>
	/// Inform any Link class of packet loss detection,
	/// due to skipped counter.
	/// </summary>
	/// <param name="lostCount"></param>
	/// <returns></returns>
	virtual void OnReceiveLossDetected(const uint8_t lostCount)
	{
		OnEvent(PacketEventEnum::ReceiveLossDetected);
	}

	/// <summary>
	/// Calculates a re-send delay, with a random jitter if Duplex is not full.
	/// Since we are in a pre-link state, jitter is added for collision avoidance.
	/// </summary>
	/// <returns>Delay to apply to resend (in milliseconds).</returns>
	const uint32_t PreLinkResendDelayMillis(const uint8_t payloadSize)
	{
		const uint32_t duration = (LoLaLinkDefinition::REPLY_BASE_TIMEOUT_MICROS +
			GetSendDuration(payloadSize))
			/ 1000;

		if (Duplex->IsFullDuplex())
		{
			return duration;
		}
		else
		{

			return duration + Session.RandomSource.GetRandomLong(duration);
		}
	}

	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			break;
		case LinkStageEnum::AwaitingLink:
			Session.RandomSource.RandomReseed();
			LastReceivedRssi = 0;
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			LinkStartSeconds = SyncClock.GetSeconds(0);
			ResetLastValidReceived();
			RequestReportUpdate();
			break;
		default:
			break;
		}

#if defined(DEBUG_LOLA)
		this->Owner();

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			Serial.println(F("Stage = Disabled"));
			break;
		case LinkStageEnum::Booting:
			Serial.println(F("Stage = Booting"));
			break;
		case LinkStageEnum::AwaitingLink:
			Serial.println(F("Stage = AwaitingLink"));
			break;
		case LinkStageEnum::Linking:
			Serial.println(F("Stage = Linking"));
			break;
		case LinkStageEnum::Linked:
			Serial.println(F("Stage = Linked"));
			break;
		default:
			break;
		}
#endif
	}

	virtual void OnService() final
	{
		if (!CheckPendingAck())
		{
			switch (LinkStage)
			{
			case LinkStageEnum::Disabled:
				SyncClock.Stop();
				Task::disable();
				break;
			case LinkStageEnum::Booting:
				Session.RandomSource.RandomReseed();
				SyncClock.Start();
				SyncClock.ShiftSeconds(Session.RandomSource.GetRandomLong());
				if (Driver->DriverStart())
				{
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
				}
				else
				{
					UpdateLinkStage(LinkStageEnum::Disabled);
				}
				break;
			case LinkStageEnum::AwaitingLink:
				OnAwaitingLink(GetStageElapsedMillis());
				break;
			case LinkStageEnum::Linking:
				OnLinking(GetStageElapsedMillis());
				break;
			case LinkStageEnum::Linked:
				if (GetElapsedSinceLastValidReceived() > LoLaLinkDefinition::LINK_STAGE_TIMEOUT)
				{
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
				}
				else if (CheckForReportUpdate())
				{
					// Sending report packet.
					// Report takes priority over clock, as it refers to counters and RSSI.
				}
				else if (CheckForClockSyncUpdate())
				{
					// Sending clock sync packet.
				}
				else
				{
					Task::enableDelayed(LINK_CHECK_PERIOD);
				}
				break;
			default:
				break;
			}
		}
	}

	virtual void OnEvent(const PacketEventEnum packetEvent)
	{
		BaseClass::OnEvent(packetEvent);


		switch (packetEvent)
		{
		case PacketEventEnum::ReceiveRejectedCounter:
			RequestReportUpdate();
			break;
		default:
			break;
		}

#if defined(DEBUG_LOLA)
		switch (packetEvent)
		{
		case PacketEventEnum::Received:
			break;
		case PacketEventEnum::ReceivedAck:
			this->Owner();
			Serial.println(F("@Link Event: Received: Ack"));
			break;
		case PacketEventEnum::ReceiveRejectedAck:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Ack"));
			break;
		case PacketEventEnum::ReceiveRejectedCounter:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Counter"));
			break;
		case PacketEventEnum::ReceiveRejectedDriver:
			//this->Owner();
			//Serial.println(F("@Link Event: ReceiveRejected: Driver"));
			break;
		case PacketEventEnum::ReceiveRejectedDropped:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Dropped."));
			break;
		case PacketEventEnum::ReceiveRejectedInvalid:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Invalid."));
			break;
		case PacketEventEnum::ReceiveRejectedOutOfSlot:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: OutOfSlot"));
			break;
		case PacketEventEnum::ReceiveRejectedMac:
			//this->Owner();
			//Serial.println(F("@Link Event: ReceiveRejectedMAC"));
			break;
		case PacketEventEnum::SendAckFailed:
			this->Owner();
			Serial.println(F("@Link Event: SendAck: Failed"));
			break;
		case PacketEventEnum::SendCollisionFailed:
			this->Owner();
			Serial.println(F("@Link Event: SendCollision: Failed"));
			break;
		case PacketEventEnum::ReceiveLossDetected:
			this->Owner();
			Serial.println(F("Link detected and recovered from lost packets."));
			break;
		case PacketEventEnum::Sent:
			break;
		case PacketEventEnum::SentAck:
			break;
		case PacketEventEnum::SentWithAck:
			break;
		default:
			break;
		}
#endif

	}

protected:
	/// <summary>
	/// Sends update Report, if needed.
	/// </summary>
	/// <returns>True if a report update is due or pending to send.</returns>
	virtual const bool CheckForReportUpdate() final
	{
		const uint32_t timestamp = millis();
		if (ReportTracking.IsRequested(timestamp))
		{
			if (CanRequestSend() && ReportTracking.IsSendRequested(timestamp))
			{
				OutPacket.SetPort(Linked::PORT);
				OutPacket.Payload[Linked::ReportUpdate::SUB_HEADER_INDEX] = Linked::ReportUpdate::SUB_HEADER;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX] = LastReceivedRssi;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] = LastValidReceivedCounter;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] = ReportTracking.IsBackReportNeeded(timestamp, GetElapsedSinceLastValidReceived());

				if (RequestSendPacket(Linked::ReportUpdate::PAYLOAD_SIZE))
				{
					ReportTracking.OnReportSent(timestamp);
				}
			}

			Task::enableIfNot();
			return true;
		}
		else if (ReportTracking.IsReportNeeded(timestamp, GetElapsedSinceLastValidReceived()))
		{
			RequestReportUpdate();
			Task::enableIfNot();
			return true;
		}

		return false;
	}

private:
	/// <summary>
	/// Schedule next report for dispatch.
	/// </summary>
	void RequestReportUpdate()
	{
		ReportTracking.RequestReportUpdate(Session.RandomSource.GetRandomShort() - INT8_MAX);
		Task::enableIfNot();
	}

	/// <summary>
	/// Calibrate CPU dependent durations.
	/// </summary>
	const bool CalibrateSendDuration()
	{
		uint32_t calibrationStart = micros();

		// Measure short (baseline) packet first.
		uint32_t start = 0;
		uint32_t shortDuration = 0;
		uint32_t longDuration = 0;

		OutPacket.SetPort(123);
		for (uint_least8_t i = 0; i < LoLaPacketDefinition::MAX_PAYLOAD_SIZE; i++)
		{
			OutPacket.Payload[i] = i + 1;
		}

		start = micros();
		uint8_t handle = 0;
		for (uint_least16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			if (!BaseClass::MockSendPacket(this, handle, OutPacket.Data, 0))
			{
				// Calibration failed.
#if defined(DEBUG_LOLA)
				Serial.print(F("Calibration failed on short test."));
#endif
				return false;
			}
			handle++;
		}
		shortDuration = ((micros() - start) / CALIBRATION_ROUNDS);

		start = micros();
		for (uint_least16_t i = 0; i < CALIBRATION_ROUNDS; i++)
		{
			uint8_t handle = 0;
			if (!BaseClass::MockSendPacket(this, handle, OutPacket.Data, LoLaPacketDefinition::MAX_PAYLOAD_SIZE))
			{
				// Calibration failed.
#if defined(DEBUG_LOLA)
				Serial.print(F("Calibration failed on long test."));
#endif
				return false;
			}
		}

		longDuration = (micros() - start) / CALIBRATION_ROUNDS;

		const uint32_t scaleDuration = longDuration - shortDuration;

		//// +1 to mitigate low bias by integer rounding.
		const uint32_t byteDurationMicros = (scaleDuration / (LoLaPacketDefinition::MAX_PAYLOAD_SIZE - LoLaPacketDefinition::PAYLOAD_INDEX)) + 1;

		//// Remove the base bytes from the base duration,
		//// as well as the remainder from (now) high-biased byte duration.
		const uint32_t baseDurationMicros = shortDuration
			- (byteDurationMicros * LoLaPacketDefinition::PAYLOAD_INDEX)
			- (scaleDuration % (LoLaPacketDefinition::MAX_PAYLOAD_SIZE - LoLaPacketDefinition::PAYLOAD_INDEX));

		const uint32_t calibrationDuration = micros() - calibrationStart;

#if defined(DEBUG_LOLA)
		SetSendCalibration(shortDuration, longDuration);
		Serial.println();
		Serial.print(F("Calibration done. (max payload size"));
		Serial.print(LoLaPacketDefinition::MAX_PAYLOAD_SIZE);
		Serial.println(')');
		Serial.println(F("Short\tLong"));
		Serial.print(shortDuration);
		Serial.print('\t');
		Serial.println(longDuration);
		Serial.println();
		Serial.println(F("Full estimation"));
		Serial.println(F("Short\tLong"));
		Serial.print(GetSendDuration(0));
		Serial.print('\t');
		Serial.println(GetSendDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
		Serial.println();
		Serial.print(F("Calibration ("));
		Serial.print(CALIBRATION_ROUNDS);
		Serial.print(F(" rounds) took "));
		Serial.print(calibrationDuration);
		Serial.println(F(" us"));
		Serial.println();
#endif

		return SetSendCalibration(shortDuration, longDuration);
	}
};

#endif