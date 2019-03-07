// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

#include <Services\Link\LoLaLinkLatencyMeter.h>


class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum : uint8_t
	{
		BroadcastingOpenSession = 0,
		ValidatingPartner = 1,
		SendingPublicKey = 2,
		ProcessingPKC = 3,
		GotSharedKey = 4,
		LinkingSwitchOver = 5
	};

	enum InfoSyncStagesEnum : uint8_t
	{
		AwaitingLatencyMeasurement = 0,
		SendingInfoRequest = 1,
		SendingHostInfo = 2
	};

	LinkHostClockSyncer ClockSyncer;
	ClockSyncResponseTransaction HostClockSyncTransaction;

	//Latency measurement.
	LoLaLinkLatencyMeter<LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES> LatencyMeter;

	//Session lifetime.
	uint32_t SessionLastStarted = ILOLA_INVALID_MILLIS;

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &HostClockSyncTransaction;
#ifdef USE_TIME_SLOT
		loLa->SetDuplexSlot(false);
#endif
	}
protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			NewSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			InfoSyncStage = InfoSyncStagesEnum::AwaitingLatencyMeasurement;
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			ClockSyncer.SetReadyForEstimation();
			HostClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnPreSend()
	{
		if (!LinkInfo->HasLink() && PacketHolder.GetDefinition()->HasACK())
		{
			//Piggy back on any link acked packets to measure latency.
			LatencyMeter.OnAckPacketSent(PacketHolder.GetId());
		}
	}

	void OnClearSession()
	{
		LatencyMeter.Reset();
		SessionLastStarted = ILOLA_INVALID_MILLIS;
	}

	///PKC region.
	bool OnAwaitingLink()
	{
		if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP)
		{
			return false;
		}
		else switch (LinkingState)
		{
		case AwaitingLinkEnum::BroadcastingOpenSession:
			if (!LinkInfo->HasSessionId() || SessionLastStarted == ILOLA_INVALID_MILLIS || millis() - SessionLastStarted > LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME)
			{
				NewSession();
				SessionLastStarted = millis();
			}

			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD)
			{
				PrepareIdBroadcast();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::ValidatingPartner:
			//TODO: Filter accepted Id from known list.
			if (true)
			{
				GetLoLa()->GetCryptoEncoder()->SetIvData(LinkInfo->GetSessionId(),
					LinkInfo->GetLocalId(), LinkInfo->GetPartnerId());
				SubStateStart = millis();
				SetLinkingState(AwaitingLinkEnum::SendingPublicKey);
			}
			else
			{
				LinkInfo->ClearRemoteId();
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			break;
		case AwaitingLinkEnum::SendingPublicKey:
			if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
			{
				ClearSession();
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD)
			{
				PreparePublicKeyPacket(LOLA_LINK_SUBHEADER_HOST_PUBLIC_KEY);
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::ProcessingPKC:
			//TODO: Solve key size issue
			//TODO: Use authorization dataas token?
			if (KeyExchanger.GenerateSharedKey() &&
				GetLoLa()->GetCryptoEncoder()->SetSecretKey(KeyExchanger.GetSharedKeyPointer(), 16))
			{
				SetLinkingState(AwaitingLinkEnum::GotSharedKey);
			}
			else
			{
				ClearSession();
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			break;
		case AwaitingLinkEnum::GotSharedKey:
			if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
			{
				ClearSession();
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareCryptoStartRequest();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::LinkingSwitchOver:
#ifdef DEBUG_LOLA
			PKCDuration = millis() - SubStateStart;
#endif
			//All set to start linking.
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
			break;
		default:
			SetLinkingState(0);
			SetNextRunASAP();
			break;
		}

		return true;
	}

	void OnLinkDiscoveryReceived()
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remotePartnerId)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::BroadcastingOpenSession &&
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId)
		{
			LinkInfo->SetPartnerId(remotePartnerId);
			SetLinkingState(AwaitingLinkEnum::ValidatingPartner);
		}
	}

	void OnRemotePublicKeyReceived(const uint8_t sessionId, uint8_t *remotePublicKey)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::SendingPublicKey &&
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId)
		{
			//The partner Public Key is encoded at this point.
			if (KeyExchanger.SetPartnerPublicKey(remotePublicKey))
			{
				SetLinkingState(AwaitingLinkEnum::ProcessingPKC);
			}
		}
	}
	///

	///Linking Stage.
	void OnLinking()
	{
		switch (LinkingState)
		{
		case LinkingStagesEnum::InfoSyncStage:
			OnInfoSync();//We move forward when we receive a clock sync request, checking if the constraints are met.
			break;
		case LinkingStagesEnum::ClockSyncStage:
			if (ClockSyncer.IsSynced())
			{
				SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
			}
			else
			{
				OnClockSync();
			}
			break;
		case LinkingStagesEnum::LinkProtocolSwitchOver:
			//We transition forward when we receive the Ack.
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkProtocolSwitchOver();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case LinkingStagesEnum::LinkingDone:
			//All linking stages complete, we have a link.
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linked);
			break;
		default:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		}
	}

	void OnInfoSync()
	{
		switch (InfoSyncStage)
		{
		case InfoSyncStagesEnum::AwaitingLatencyMeasurement:
			if (LatencyMeter.GetSampleCount() >= LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES)
			{
				//Update link info RTT.
				LinkInfo->SetRTT(LatencyMeter.GetAverageLatency());

				InfoSyncStage = InfoSyncStagesEnum::SendingInfoRequest;
				SetNextRunASAP();
			}
			else
			{	//If we don't have enough latency samples, we make more.
				if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_PING_RESEND_PERIOD)
				{
					PreparePing();
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
			}
			break;
		case InfoSyncStagesEnum::SendingInfoRequest:
			if (LinkInfo->HasPartnerRSSI())
			{
				InfoSyncStage = InfoSyncStagesEnum::SendingHostInfo;
				SetNextRunASAP();
			}
			else
			{
				if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
				{
					PrepareInfoSyncRequest();
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
			}
			break;
		case InfoSyncStagesEnum::SendingHostInfo:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareHostInfoSync();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		default:
			break;
		}
	}

	void OnRemoteInfoSyncReceived(const uint8_t rssi)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage)
		{
			LinkInfo->SetPartnerRSSINormalized(rssi);
			SetNextRunASAP();
		}
	}

	void OnClockSync()
	{
		if (HostClockSyncTransaction.IsResultReady())
		{
			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

			PrepareClockSyncResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
			HostClockSyncTransaction.Reset();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	///Clock Sync.
	void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking)
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));

			switch (LinkingState)
			{
			case LinkingStagesEnum::InfoSyncStage:
				if (LatencyMeter.GetSampleCount() >= LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES &&
					LinkInfo->HasPartnerRSSI())
				{
					SetLinkingState(LinkingStagesEnum::ClockSyncStage);
				}
				break;
			case LinkingStagesEnum::ClockSyncStage:
				SetNextRunASAP();
				break;
			default:
				HostClockSyncTransaction.Reset();
				break;
			}
		}
	}

	void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo->HasLink())
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
			SetNextRunASAP();
		}
	}

	void OnKeepingLink()
	{
		if (HostClockSyncTransaction.IsResultReady())
		{
			if (HostClockSyncTransaction.GetResult() == 0)
			{
				ClockSyncer.StampSynced();
			}
#ifdef DEBUG_LOLA
			else
			{
				LinkInfo->StampClockSyncAdjustment();
			}
#endif

			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

			PrepareClockSyncTuneResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
			HostClockSyncTransaction.Reset();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	void OnPingAckReceived(const uint8_t id)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking)
		{
			LatencyMeter.OnAckReceived(id);
		}
	}

	void OnLinkAckReceived(const uint8_t header, const uint8_t id)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			LatencyMeter.OnAckReceived(id);
			if (header == LOLA_LINK_HEADER_SHORT_WITH_ACK &&
				LinkingState == AwaitingLinkEnum::GotSharedKey &&
				LinkInfo->GetSessionId() == id)
			{
				SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			LatencyMeter.OnAckReceived(id);
			if (LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver &&
				header == LOLA_LINK_HEADER_SHORT_WITH_ACK &&
				LinkInfo->GetSessionId() == id)
			{
				SetLinkingState(LinkingStagesEnum::LinkingDone);
			}
			break;
		default:
			break;
		}
	}

private:
	void NewSession()
	{
		ClearSession();

		LinkInfo->SetSessionId((uint8_t)random(1, (UINT8_MAX - 1)));
	}

	void PrepareIdBroadcast()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST);
		ATUI_S.uint = LinkInfo->GetLocalId();
		S_ArrayToPayload();
	}

	void PrepareCryptoStartRequest()
	{
		PrepareLinkProtocolSwitchOver();

		GetLoLa()->GetCryptoEncoder()->Encode(PacketHolder.GetPayload(), sizeof(uint32_t));
	}

	void PrepareHostInfoSync()
	{
		PrepareReportPacket(LOLA_LINK_SUBHEADER_INFO_SYNC_HOST);
		PacketHolder.GetPayload()[0] = LinkInfo->GetRSSINormalized();
		PacketHolder.GetPayload()[1] = LinkInfo->GetRTT() & 0xFF; //MSB 16 bit unsigned.
		PacketHolder.GetPayload()[2] = (LinkInfo->GetRTT() >> 8) & 0xFF;
		for (uint8_t i = 3; i < LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT; i++)
		{
			PacketHolder.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareInfoSyncRequest()
	{
		PrepareReportPacket(LOLA_LINK_SUBHEADER_INFO_SYNC_REQUEST);
		for (uint8_t i = 3; i < LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT; i++)
		{
			PacketHolder.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareClockSyncResponse(const uint8_t requestId, const int32_t estimationError)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_REPLY);
		ATUI_S.iint = estimationError;
		S_ArrayToPayload();
	}

	void PrepareLinkProtocolSwitchOver()
	{
		PrepareShortPacketWithAck(LinkInfo->GetSessionId());
		ATUI_S.uint = LinkInfo->GetPartnerId();
		PacketHolder.GetPayload()[0] = ATUI_S.array[0];
		PacketHolder.GetPayload()[1] = ATUI_S.array[1];
		PacketHolder.GetPayload()[2] = ATUI_S.array[2];
		PacketHolder.GetPayload()[3] = ATUI_S.array[3];
	}

	void PrepareClockSyncTuneResponse(const uint8_t requestId, const int32_t estimationError)
	{		
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY);
		ATUI_S.iint = estimationError;
		S_ArrayToPayload();
	}
};
#endif