// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

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
		SendingInfoRequest = 0,
		SendingHostInfo = 1
	};

	LinkHostClockSyncer ClockSyncer;
	ClockSecondsResponseTransaction HostClockSecondsTransaction;
	ClockSyncResponseTransaction HostClockSyncTransaction;


	//Session lifetime.
	uint32_t SessionLastStarted = 0;


public:
	LoLaLinkHostService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: LoLaLinkService(servicesScheduler, driverScheduler, driver)
		, ClockSyncer()
		, HostClockSecondsTransaction()
		, HostClockSyncTransaction()	
	{
		ClockSyncerPointer = &ClockSyncer;
		driver->SetDuplexSlot(true);
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA

protected:
	void OnClearSession()
	{
		HostClockSyncTransaction.Reset();
		HostClockSecondsTransaction.Reset();
		ClockSyncer.Reset();

		SessionLastStarted = 0;
	}

	void OnLinkStateChanged(const LinkStatus::StateEnum newState)
	{
		switch (newState)
		{
		case LinkStatus::StateEnum::AwaitingLink:
			NewSession();
			break;
		case LinkStatus::StateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD);
			break;
		case LinkStatus::StateEnum::Linking:
			InfoSyncStage = InfoSyncStagesEnum::SendingInfoRequest;
			break;
		case LinkStatus::StateEnum::Linked:
			HostClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	inline void RestartAwaitingLink() 
	{
		SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
		SetNextRunDelay(random(0, LOLA_LINK_SERVICE_LINKED_UPDATE_RANDOM_JITTER_MAX));
	}

	///PKC region.
	bool OnAwaitingLink()
	{
		if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP)
		{
			return false;
		}
		else switch (LinkingState)
		{
		case AwaitingLinkEnum::BroadcastingOpenSession:
			if (!LinkInfo.HasSessionId() || SessionLastStarted == 0 || millis() - SessionLastStarted > LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME)
			{
				NewSession();
				SessionLastStarted = millis();
			}

			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD)
			{
				PrepareIdBroadcast();
				RequestSendPacket();
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
				LoLaDriver->GetCryptoEncoder()->SetIvData(LinkInfo.GetSessionId(),
					LinkInfo.GetLocalId(), LinkInfo.GetPartnerId());
				SubStateStart = millis();
				SetLinkingState(AwaitingLinkEnum::SendingPublicKey);
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
				DebugTask.OnPKCStarted();
#endif
			}
			else
			{
				LinkInfo.ClearRemoteId();
				RestartAwaitingLink();
			}
			break;
		case AwaitingLinkEnum::SendingPublicKey:
			if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
			{
				ClearSession();
				RestartAwaitingLink();
			}
			else if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD)
			{
				PreparePublicKeyPacket(LOLA_LINK_SUBHEADER_HOST_PUBLIC_KEY);
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::ProcessingPKC:
			//TODO: Solve key size issue
			//TODO: Use authorization data as token?
			if (KeyExchanger.GenerateSharedKey() &&
				LoLaDriver->GetCryptoEncoder()->SetSecretKey(KeyExchanger.GetSharedKeyPointer(), LoLaCryptoKeyExchanger::KEY_CURVE_SIZE))
			{
				SetLinkingState(AwaitingLinkEnum::GotSharedKey);
			}
			else
			{
				ClearSession();
				RestartAwaitingLink();
			}
			break;
		case AwaitingLinkEnum::GotSharedKey:
			if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
			{
				ClearSession();
				RestartAwaitingLink();
			}
			else if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareCryptoStartRequest();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::LinkingSwitchOver:
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.OnPKCDone();
#endif
			//All set to start linking.
			UpdateLinkState(LinkStatus::StateEnum::Linking);
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
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remotePartnerId)
	{
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::BroadcastingOpenSession &&
			LinkInfo.HasSessionId() &&
			LinkInfo.GetSessionId() == sessionId)
		{
			LinkInfo.SetPartnerId(remotePartnerId);
			SetLinkingState(AwaitingLinkEnum::ValidatingPartner);
		}
	}

	void OnRemotePublicKeyReceived(const uint8_t sessionId, uint8_t *remotePublicKey)
	{
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::SendingPublicKey &&
			LinkInfo.HasSessionId() &&
			LinkInfo.GetSessionId() == sessionId)
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
			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkProtocolSwitchOver();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case LinkingStagesEnum::LinkingDone:
			//All linking stages complete, we have a link.
			UpdateLinkState(LinkStatus::StateEnum::Linked);
			break;
		default:
			UpdateLinkState(LinkStatus::StateEnum::AwaitingLink);
			break;
		}
	}

	void OnInfoSync()
	{
		switch (InfoSyncStage)
		{
		case InfoSyncStagesEnum::SendingInfoRequest:
			if (LinkInfo.HasPartnerRSSI())
			{
				InfoSyncStage = InfoSyncStagesEnum::SendingHostInfo;
				SetNextRunASAP();
			}
			else if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareInfoSyncRequest();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncStagesEnum::SendingHostInfo:
			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareHostInfoSync();
				RequestSendPacket();
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
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage)
		{
			LinkInfo.SetPartnerRSSINormalized(rssi);
			SetNextRunASAP();
		}
	}

	void OnClockSync()
	{
		if (HostClockSecondsTransaction.IsRequested())
		{
			PrepareClockUTCResponse(HostClockSecondsTransaction.GetId(), SyncedClock.GetSyncSeconds());
			HostClockSecondsTransaction.Reset();
			RequestSendPacket();
		}
		else if (HostClockSyncTransaction.IsResultReady())
		{
			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());
			PrepareClockSyncResponse(HostClockSyncTransaction.GetId(), HostClockSyncTransaction.GetResult());
			HostClockSyncTransaction.Reset();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	///Clock Sync.
	void OnClockUTCRequestReceived(const uint8_t requestId)
	{
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::Linking)
		{
			switch (LinkingState)
			{
			case LinkingStagesEnum::InfoSyncStage:
				if (LinkInfo.HasPartnerRSSI())
				{
					HostClockSecondsTransaction.SetRequested(requestId);
					SetLinkingState(LinkingStagesEnum::ClockSyncStage);
				}
				break;
			case LinkingStagesEnum::ClockSyncStage:
				if (HostClockSyncTransaction.IsClear())
				{
					HostClockSecondsTransaction.SetRequested(requestId);
					SetNextRunASAP();
				}
				break;
			default:
				break;
			}
		}
	}


	void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros)
	{
		if (LinkInfo.GetLinkState() == LinkStatus::StateEnum::Linking)
		{
			HostClockSyncTransaction.SetResult(requestId, SyncedClock.GetSyncMicros() + (micros() - LoLaDriver->GetLastValidReceivedMicros()) - estimatedMicros);
			SetNextRunASAP();

			switch (LinkingState)
			{
			case LinkingStagesEnum::InfoSyncStage:
				if (LinkInfo.HasPartnerRSSI())
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

	void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros)
	{
		if (LinkInfo.HasLink())
		{
			HostClockSyncTransaction.SetResult(requestId, SyncedClock.GetSyncMicros() + (micros() - LoLaDriver->GetLastValidReceivedMicros()) - estimatedMicros);
			SetNextRunASAP();
		}
	}

	void OnKeepingLink()
	{
		if (HostClockSyncTransaction.IsResultReady())
		{
			if (!ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult()))
			{
				LinkInfo.StampClockSyncAdjustment();
			}
			PrepareClockSyncTuneResponse(HostClockSyncTransaction.GetId(), HostClockSyncTransaction.GetResult());
			HostClockSyncTransaction.Reset();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	

	virtual void OnLinkAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (header != DefinitionShortWithAck.GetHeader())
		{
			return;
		}

		switch (LinkInfo.GetLinkState())
		{
		case LinkStatus::StateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::GotSharedKey &&
				LinkInfo.GetSessionId() == id)
			{
				SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);
			}
			break;
		case LinkStatus::StateEnum::Linking:
			if (LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver &&
				LinkInfo.GetSessionId() == id)
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

		LinkInfo.SetSessionId((uint8_t)random(1, (UINT8_MAX - 1)));
	}

	void PrepareIdBroadcast()
	{
		PrepareShortPacket(LinkInfo.GetSessionId(), LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST);
		ATUI_S.uint = LinkInfo.GetLocalId();
		S_ArrayToPayload();
	}

	void PrepareCryptoStartRequest()
	{
		PrepareLinkProtocolSwitchOver();
		LoLaDriver->GetCryptoEncoder()->EncodeDirect(OutPacket.GetPayload(), sizeof(uint32_t));
	}

	void PrepareHostInfoSync()
	{
		PrepareReportPacket(LOLA_LINK_SUBHEADER_INFO_SYNC_HOST);
		OutPacket.GetPayload()[0] = LinkInfo.GetRSSINormalized();
		for (uint8_t i = 3; i < DefinitionReport.GetPayloadSize(); i++)
		{
			OutPacket.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareInfoSyncRequest()
	{
		PrepareReportPacket(LOLA_LINK_SUBHEADER_INFO_SYNC_REQUEST);
		for (uint8_t i = 0; i < DefinitionReport.GetPayloadSize(); i++)
		{
			OutPacket.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareClockUTCResponse(const uint8_t requestId, const uint32_t secondsUTC)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_UTC_REPLY);
		ATUI_S.uint = secondsUTC;
		S_ArrayToPayload();
	}

	void PrepareClockSyncResponse(const uint8_t requestId, const int32_t estimationErrorMicros)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_REPLY);
		ATUI_S.iint = estimationErrorMicros;
		S_ArrayToPayload();
	}

	void PrepareLinkProtocolSwitchOver()
	{
		PrepareShortPacketWithAck(LinkInfo.GetSessionId());
		ATUI_S.uint = LinkInfo.GetPartnerId();
		OutPacket.GetPayload()[0] = ATUI_S.array[0];
		OutPacket.GetPayload()[1] = ATUI_S.array[1];
		OutPacket.GetPayload()[2] = ATUI_S.array[2];
		OutPacket.GetPayload()[3] = ATUI_S.array[3];
	}

	void PrepareClockSyncTuneResponse(const uint8_t requestId, const int32_t estimationError)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY);
		ATUI_S.iint = estimationError;
		S_ArrayToPayload();
	}
};
#endif