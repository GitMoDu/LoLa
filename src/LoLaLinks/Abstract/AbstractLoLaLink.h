// AbstractLoLaLink.h
#ifndef _ABSTRACT_LOLA_LINK_
#define _ABSTRACT_LOLA_LINK_


#include "AbstractLoLaLinkPacket.h"
#include "../../Link/LinkDefinitions.h"
#include "../../Link/ReportTracker.h"

using namespace LinkDefinitions;

/// <summary>
/// 
/// </summary>
class AbstractLoLaLink : public AbstractLoLaLinkPacket
{
private:
	using BaseClass = AbstractLoLaLinkPacket;

private:
	/// <summary>
	/// Slow value, let the main services hog the CPU during Link time.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;

protected:
	ReportTracker QualityTracker;

private:
	uint32_t LastUnlinkedSent = 0;

	Timestamp LinkStartTimestamp{};

protected:
	/// <summary>
	/// </summary>
	virtual void OnServiceAuthenticating() {}
	virtual void OnServiceClockSyncing() {}


	virtual void OnServiceSleeping() {}
	virtual void OnServiceSearching() {}
	virtual void OnServicePairing() {}
	virtual void OnServiceSwitchingToLinking() {}
	virtual void OnServiceSwitchingToLinked() {}

	/// <summary>
	/// </summary>
	/// <returns>True if a clock sync update is due or pending to send.</returns>
	virtual const bool CheckForClockTuneUpdate() { return false; }

	virtual const uint8_t GetClockQuality() { return 0; }

public:
	AbstractLoLaLink(TS::Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, linkRegistry, transceiver, cycles, entropy, duplex, hop)
		, QualityTracker(LoLaLinkDefinition::GetLinkTimeoutDuration(duplex->GetPeriod()))
	{}

	virtual const bool Setup()
	{
		if (Registry->RegisterPacketListener(this, LoLaLinkDefinition::LINK_PORT))
		{
			return BaseClass::Setup();
		}
#if defined(DEBUG_LOLA)
		else
		{
			this->Owner();
			Serial.println(F("Register Link Port failed."));
		}
#endif

		return false;
	}

	void GetLinkStatus(LoLaLinkStatus& linkStatus) final
	{
		linkStatus.RxDropCount = QualityTracker.GetRxDropCount();
		linkStatus.TxCount = SentCounter;
		linkStatus.RxCount = ReceivedCounter;
		linkStatus.RxDropRate = QualityTracker.GetRxDropRate();
		linkStatus.TxDropRate = QualityTracker.GetTxDropRate();
		linkStatus.Quality.RxRssi = QualityTracker.GetRxRssiQuality();
		linkStatus.Quality.TxRssi = QualityTracker.GetTxRssiQuality();
		linkStatus.Quality.RxDrop = QualityTracker.GetRxDropQuality();
		linkStatus.Quality.TxDrop = QualityTracker.GetTxDropQuality();
		linkStatus.Quality.ClockSync = GetClockQuality();
		linkStatus.Quality.Age = QualityTracker.GetLastValidReceivedAgeQuality();

		SyncClock.GetTimestampMonotonic(LinkTimestamp);
		linkStatus.DurationSeconds = Timestamp::GetElapsedSeconds(LinkStartTimestamp, LinkTimestamp);
	}

	const uint32_t GetLinkElapsed() final
	{
		if (HasLink())
		{
			SyncClock.GetTimestampMonotonic(LinkTimestamp);
			return LinkTimestamp.GetRollingMicros() - LinkStartTimestamp.GetRollingMicros();
		}
		else
		{
			return 0;
		}
	}

	void OnSendComplete(const IPacketServiceListener::SendResultEnum result) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Searching:
		case LinkStageEnum::Pairing:
		case LinkStageEnum::SwitchingToLinking:
		case LinkStageEnum::Authenticating:
		case LinkStageEnum::ClockSyncing:
		case LinkStageEnum::SwitchingToLinked:
			if (result == IPacketServiceListener::SendResultEnum::Success)
			{
				LastUnlinkedSent = micros();
			}
			break;
		default:
			break;
		}
	}

	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port)
	{
		if (port == LoLaLinkDefinition::LINK_PORT
			&& payload[HeaderDefinition::HEADER_INDEX] == Linked::ReportUpdate::HEADER
			&& payloadSize == Linked::ReportUpdate::PAYLOAD_SIZE)
		{
			const uint16_t loopingDropCounter = payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX]
				| ((uint16_t)payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX + 1] << 8);

			// Keep track of partner's RSSI and packet drop, for output gain management.
			QualityTracker.OnReportReceived(micros(),
				loopingDropCounter,
				payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX],
				payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX]);
		}
#if defined(DEBUG_LOLA_LINK)
		else
		{
			// This is the top-most class to parse incoming packets as a consumer.
			// So the base class call is not needed here.
			this->Skipped(F("Unknown"));
		}
#endif
	}

	const bool Start() final
	{
		if (LinkStage == LinkStageEnum::Disabled)
		{
			UpdateLinkStage(LinkStageEnum::Booting);

			return true;
		}
		else
		{
			return false;
		}
	}

	const bool Stop() final
	{
		if (LinkStage != LinkStageEnum::Disabled)
		{
			UpdateLinkStage(LinkStageEnum::Disabled);
			return Transceiver->Stop();
		}
		else
		{
			return false;
		}
	}

protected:
	void OnPacketReceivedOk(const uint8_t rssi, const uint16_t lostCount) final
	{
		QualityTracker.OnRxComplete(micros(), rssi, lostCount);
	}

protected:
	void ResetUnlinkedPacketThrottle()
	{
		LastUnlinkedSent -= GetPacketThrottlePeriod() * 2;
	}

	const bool UnlinkedPacketThrottle()
	{
		return (micros() - LastUnlinkedSent) >= GetPacketThrottlePeriod() * 2;
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			SyncClock.Stop();
			break;
		case LinkStageEnum::Booting:
			SyncClock.Start();
			SyncClock.ShiftSubSeconds(RandomSource.GetRandomLong());
			break;
		case LinkStageEnum::Searching:
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
			break;
		case LinkStageEnum::Authenticating:
			QualityTracker.ResetRssiQuality();
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestampMonotonic(LinkStartTimestamp);
			QualityTracker.Reset(micros());
			break;
		default:
			break;
		}

		BaseClass::UpdateLinkStage(linkStage);

#if defined(DEBUG_LOLA_LINK)
		this->Owner();

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			Serial.println(F("Stage = Disabled"));
			break;
		case LinkStageEnum::Booting:
			Serial.println(F("Stage = Booting"));
			break;
		case LinkStageEnum::Sleeping:
			Serial.println(F("Stage = Sleeping"));
			break;
		case LinkStageEnum::Searching:
			Serial.println(F("Stage = Searching"));
			break;
		case LinkStageEnum::Pairing:
			Serial.println(F("Stage = Pairing"));
			break;
		case LinkStageEnum::SwitchingToLinking:
			Serial.println(F("Stage = SwitchingToLinking"));
			break;
		case LinkStageEnum::Authenticating:
			Serial.println(F("Stage = Authenticating"));
			break;
		case LinkStageEnum::ClockSyncing:
			Serial.println(F("Stage = ClockSyncing"));
			break;
		case LinkStageEnum::SwitchingToLinked:
			Serial.println(F("Stage = SwitchingToLinked"));
			break;
		case LinkStageEnum::Linked:
			Serial.println(F("Stage = Linked"));
			break;
		default:
			break;
		}
#endif
	}

	virtual void OnServiceRun() final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			TS::Task::disable();
			break;
		case LinkStageEnum::Sleeping:
			OnServiceSleeping();
			break;
		case LinkStageEnum::Booting:
			if (Transceiver->Start())
			{
				UpdateLinkStage(LinkStageEnum::Searching);
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Link failed to start transceiver"));
#endif
				UpdateLinkStage(LinkStageEnum::Disabled);
			}
			break;
		case LinkStageEnum::Searching:
			if (GetStageElapsed() > 10000000)
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Search timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::Sleeping);
			}
			else
			{
				OnServiceSearching();
			}
			break;
		case LinkStageEnum::Pairing:
			if (GetStageElapsed() > GetLinkingStageTimeoutDuration())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Pairing timed out!"));
#endif
				//TODO restart pairing.
				UpdateLinkStage(LinkStageEnum::Searching);
			}
			else
			{
				OnServicePairing();
			}
			break;
		case LinkStageEnum::SwitchingToLinking:
			OnServiceSwitchingToLinking();
			break;
		case LinkStageEnum::Authenticating:
			if (GetStageElapsed() > GetLinkingStageTimeoutDuration())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Authentication timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::Searching);
			}
			else
			{
				OnServiceAuthenticating();
			}
			break;
		case LinkStageEnum::ClockSyncing:
			if (GetStageElapsed() > GetLinkingStageTimeoutDuration())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("ClockSync timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::Searching);
			}
			else
			{
				OnServiceClockSyncing();
			}
			break;
		case LinkStageEnum::SwitchingToLinked:
			OnServiceSwitchingToLinked();
			break;
		case LinkStageEnum::Linked:
			if (QualityTracker.GetLastValidReceivedAgeQuality() == 0)
			{
				// Zero quality age means link has timed out.
				UpdateLinkStage(LinkStageEnum::Searching);
			}
			else if (CheckForReportUpdate())
			{
				// Report takes priority over clock, as it refers to counters and RSSI.
				CheckForClockTuneUpdate();
				TS::Task::enable();
			}
			else if (CheckForClockTuneUpdate())
			{
				TS::Task::enable();
			}
			else
			{
				TS::Task::enableDelayed(LINK_CHECK_PERIOD);
			}
			break;
		default:
			break;
		}
	}

#if defined(DEBUG_LOLA_LINK)
	virtual void OnEvent(const PacketEventEnum packetEvent)
	{
		this->Owner();
		switch (packetEvent)
		{
		case PacketEventEnum::ReceiveRejectedHeader:
			Serial.println(F("@Link Event: ReceiveRejected: Header/Counter"));
			break;
		case PacketEventEnum::ReceiveRejectedMac:
			Serial.println(F("@Link Event: ReceiveRejectedMAC"));
			break;
		case PacketEventEnum::SendCollisionFailed:
			Serial.println(F("@Link Event: SendCollision: Failed"));
			break;
		default:
			Serial.println(F("@Link Event: Unknown."));
			break;
		}
	}
#endif

private:
	/// <summary>
	/// Sends update Report, if needed.
	/// </summary>
	/// <returns>True if a report update is due or pending to send.</returns>
	const bool CheckForReportUpdate()
	{
		const uint32_t timestamp = micros();

		if (QualityTracker.CheckUpdate(timestamp))
		{
			if (QualityTracker.IsTimeToSendReport(timestamp)
				&& CanRequestSend())
			{
				const uint16_t rxLoopingDropCounter = QualityTracker.GetRxLoopingDropCount();
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.Payload[Linked::ReportUpdate::HEADER_INDEX] = Linked::ReportUpdate::HEADER;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] = QualityTracker.IsBackReportNeeded(timestamp);
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX] = QualityTracker.GetRxRssiQuality();
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX] = rxLoopingDropCounter;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX + 1] = rxLoopingDropCounter >> 8;

				if (RequestSendPacket(
					Linked::ReportUpdate::PAYLOAD_SIZE,
					GetReportPriority(QualityTracker.GetRxDropQuality(), QualityTracker.GetTxDropQuality())))
				{
					QualityTracker.OnReportSent(timestamp);
				}
			}

			return true;
		}

		return false;
	}

	static const RequestPriority GetReportPriority(const uint8_t rxDropQuality, const uint8_t txDropQuality)
	{
		uint8_t measure;
		if (rxDropQuality < txDropQuality)
		{
			measure = UINT8_MAX - rxDropQuality;
		}
		else
		{
			measure = UINT8_MAX - txDropQuality;
		}

		if (measure >= (UINT8_MAX / 4))
		{
			measure = UINT8_MAX;
		}
		else
		{
			measure *= 4;
		}

		return GetProgressPriority<RequestPriority::IRREGULAR, RequestPriority::RESERVED_FOR_LINK>(measure);
	}
};
#endif