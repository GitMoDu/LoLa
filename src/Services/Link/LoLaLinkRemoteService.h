// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum
	{
		SearchingForHost = 0,
		ValidatingPartner = 1,
		AwaitingHostPublicKey = 2,
		ProcessingSharedKey = 3,
		SendingPublicKey = 4,
		LinkingSwitchOver = 5
	};

	enum InfoSyncStagesEnum : uint8_t
	{
		AwaitingHostRequest = 0,
		SendingRemoteInfo = 1,
		InfoSyncDone = 2
	};

	LinkRemoteClockSyncer ClockSyncer;

	ClockSecondsRequestTransaction RemoteClockSecondsTransaction;
	ClockSyncRequestTransaction RemoteClockSyncTransaction;

public:
	LoLaLinkRemoteService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: LoLaLinkService(servicesScheduler, driverScheduler, driver)
		, ClockSyncer()
		, RemoteClockSecondsTransaction()
		, RemoteClockSyncTransaction()
	{
		ClockSyncerPointer = &ClockSyncer;
		driver->SetDuplexSlot(false);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			KeyExchanger.GenerateNewKeyPair();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			InfoSyncStage = InfoSyncStagesEnum::AwaitingHostRequest;
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			ClockSyncer.SetReadyForEstimation();
			break;
		default:
			break;
		}
	}

	void OnPreSend()
	{
		if (OutPacket.GetDataHeader() == DefinitionShort.GetHeader())
		{
			//If we are sending a clock sync request, we update our synced clock payload as late as possible.
			if (OutPacket.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_REQUEST)
			{
				ATUI_S.uint = LoLaDriver->GetClockSource()->GetSyncMicrosFull()
					+ LoLaDriver->GetETTMMicros();
				S_ArrayToPayload();
			}
			else if (OutPacket.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST)
			{
				ATUI_S.uint = LoLaDriver->GetClockSource()->GetSyncMicros()
					+ LoLaDriver->GetETTMMicros();
				S_ArrayToPayload();
			}
		}
	}

	void OnClearSession()
	{
		ClockSyncer.Reset();
		RemoteClockSyncTransaction.Reset();
		RemoteClockSecondsTransaction.Reset();
	}

	bool OnAwaitingLink()
	{
		if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_REMOTE_MAX_BEFORE_SLEEP)
		{
			return false;
		}
		else
		{
			switch (LinkingState)
			{
			case AwaitingLinkEnum::SearchingForHost:
				if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_REMOTE_SEARCH_PERIOD)
				{
					//Send an Hello to wake up potential hosts.
					PrepareLinkDiscovery();
					RequestSendPacket();
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				break;
			case AwaitingLinkEnum::ValidatingPartner:
				//TODO: Filter accepted hosts by Id.
				if (true)
				{
					LoLaDriver->GetCryptoEncoder()->SetIvData(LinkInfo->GetSessionId(),
						LinkInfo->GetPartnerId(), LinkInfo->GetLocalId());
					ResetLastSentTimeStamp();
					SubStateStart = millis();
					SetLinkingState(AwaitingLinkEnum::AwaitingHostPublicKey);
				}
				else
				{
					ClearSession();
					SetLinkingState(AwaitingLinkEnum::SearchingForHost);
				}
				break;
			case AwaitingLinkEnum::AwaitingHostPublicKey:
				if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
				{
					ClearSession();
					SetLinkingState(AwaitingLinkEnum::SearchingForHost);
				}
				else if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
				{
					PreparePKCStartRequest();
					RequestSendPacket();
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				break;
			case AwaitingLinkEnum::ProcessingSharedKey:
				//TODO: Solve key size issue
				if (KeyExchanger.GenerateSharedKey() &&
					LoLaDriver->GetCryptoEncoder()->SetSecretKey(KeyExchanger.GetSharedKeyPointer(), LoLaCryptoKeyExchanger::KEY_CURVE_SIZE))
				{
					ResetLastSentTimeStamp();
					SetLinkingState(AwaitingLinkEnum::SendingPublicKey);
				}
				else
				{
					ClearSession();
					SetLinkingState(AwaitingLinkEnum::SearchingForHost);
				}
				break;
			case AwaitingLinkEnum::SendingPublicKey:
				if (millis() - SubStateStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL)
				{
					ClearSession();
					SetLinkingState(AwaitingLinkEnum::SearchingForHost);
				}
				else if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD)
				{
					PreparePublicKeyPacket(LOLA_LINK_SUBHEADER_REMOTE_PUBLIC_KEY);
					RequestSendPacket();
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
				break;
			}
		}

		return true;
	}
	void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostId)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			switch (LinkingState)
			{
			case AwaitingLinkEnum::SearchingForHost:
				if (!LinkInfo->HasSession() && LinkInfo->SetSessionId(sessionId))
				{
					LinkInfo->SetPartnerId(hostId);
					SetLinkingState(AwaitingLinkEnum::ValidatingPartner);
				}
				break;
			case AwaitingLinkEnum::ValidatingPartner:
			case AwaitingLinkEnum::AwaitingHostPublicKey:
				//In case we have a pending link and our target host has a new session.
				//Note: this is an easy target for denial of service.
				if (LinkInfo->HasSession() &&
					LinkInfo->GetPartnerId() == hostId &&
					LinkInfo->SetSessionId(sessionId))
				{
					SetLinkingState(AwaitingLinkEnum::ValidatingPartner);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	void OnHostPublicKeyReceived(const uint8_t sessionId, uint8_t* hostPublicKey)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::AwaitingHostPublicKey &&
			LinkInfo->HasSession() &&
			LinkInfo->GetSessionId() == sessionId)
		{
			//Assumes public key is the correct size.
			if (KeyExchanger.SetPartnerPublicKey(hostPublicKey))
			{
				SetLinkingState(AwaitingLinkEnum::ProcessingSharedKey);
			}
		}
	}

	bool OnLinkProtocolReceived(const uint8_t sessionId, uint32_t hostId)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage &&
			LinkInfo->GetSessionId() == sessionId &&
			LinkInfo->GetLocalId() == hostId)
		{
			SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);

			return true;
		}

		return false;
	}

	bool OnCryptoStartReceived(const uint8_t sessionId, uint8_t* localId)
	{
		if (LinkingState == AwaitingLinkEnum::SendingPublicKey &&
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId)
		{
			///Quick decode for validation.
			LoLaDriver->GetCryptoEncoder()->DecodeDirect(localId, sizeof(uint32_t), ATUI_R.array);

			if (LinkInfo->GetLocalId() == ATUI_R.uint)
			{
				SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);

				return true;
			}
		}

		return false;
	}

	///Likning region.
	void OnLinking()
	{
		switch (LinkingState)
		{
		case LinkingStagesEnum::InfoSyncStage:
			if (OnInfoSync())
			{
				SetLinkingState(LinkingStagesEnum::ClockSyncStage);
			}
			break;
		case LinkingStagesEnum::ClockSyncStage:
			OnClockSync();//We transition forward when we receive the protocol switch over.
			break;
		case LinkingStagesEnum::LinkProtocolSwitchOver:
			ClockSyncer.SetSynced();
			SetLinkingState(LinkingStagesEnum::LinkingDone);
			SetNextRunDelay(LoLaDriver->GetETTMMicros() / (uint32_t)1000);//No point in switching to linked, before the host has had a chance to receive the ack.
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

	bool OnInfoSync()
	{
		switch (InfoSyncStage)
		{
		case InfoSyncStagesEnum::AwaitingHostRequest:
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			break;
		case InfoSyncStagesEnum::SendingRemoteInfo:
			//First move is done by remote, sending our update until we receive host's.
			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareRemoteInfoSync();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncStagesEnum::InfoSyncDone:
			return true;
		default:
			break;
		}

		return false;
	}

	void OnHostInfoSyncRequestReceived()
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage &&
			InfoSyncStage == InfoSyncStagesEnum::AwaitingHostRequest)
		{
			InfoSyncStage = InfoSyncStagesEnum::SendingRemoteInfo;
			SetNextRunASAP();
		}
	}

	void OnHostInfoSyncReceived(const uint8_t rssi, const uint16_t rtt)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage &&
			InfoSyncStage == InfoSyncStagesEnum::SendingRemoteInfo)
		{
			LinkInfo->SetRTT(rtt);
			LinkInfo->SetPartnerRSSINormalized(rssi);

			InfoSyncStage = InfoSyncStagesEnum::InfoSyncDone;
			SetNextRunASAP();
		}
	}

	void OnClockSync()
	{
		if (RemoteClockSecondsTransaction.IsResultWaiting())
		{
			ClockSyncer.OnSyncSecondsReceived(RemoteClockSecondsTransaction.GetResult());
			RemoteClockSecondsTransaction.SetDone();
			SetNextRunASAP();
		}
		else if (RemoteClockSyncTransaction.IsResultWaiting())
		{
			ClockSyncer.OnEstimationErrorReceived(RemoteClockSyncTransaction.GetResult());
			RemoteClockSyncTransaction.Reset();
			SetNextRunASAP();
		}
		else if (RemoteClockSecondsTransaction.IsDone())
		{
			if ((!RemoteClockSyncTransaction.IsRequested() ||
				!RemoteClockSyncTransaction.IsFresh(LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME)) &&
				GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				RemoteClockSyncTransaction.Reset();
				RemoteClockSyncTransaction.SetRequested();
				PrepareClockSyncRequest(RemoteClockSyncTransaction.GetId());
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
		}
		else if (!RemoteClockSecondsTransaction.IsRequested() ||
			!RemoteClockSecondsTransaction.IsFresh(LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME))
		{
			RemoteClockSecondsTransaction.Reset();
			RemoteClockSecondsTransaction.SetRequested();
			PrepareClockUTCRequest(RemoteClockSecondsTransaction.GetId());
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	void OnClockUTCResponseReceived(const uint8_t requestId, const uint32_t secondsUTC)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage &&
			RemoteClockSecondsTransaction.IsRequested() &&
			RemoteClockSecondsTransaction.IsFresh(LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME))
		{
			if (RemoteClockSecondsTransaction.SetResult(requestId, secondsUTC))
			{
				SetNextRunASAP();
			}
		}
	}

	void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedErrorMicros)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.IsFresh(LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME))
		{
			if (RemoteClockSyncTransaction.SetResult(requestId, estimatedErrorMicros))
			{
				Serial.print(F(" @ Got: "));
				Serial.println(estimatedErrorMicros);
				SetNextRunASAP();
			}
		}
	}
	///

	///Linked region.
	void OnKeepingLink()
	{
		if (RemoteClockSyncTransaction.IsResultWaiting())
		{
			if (!ClockSyncer.OnTuneErrorReceived(RemoteClockSyncTransaction.GetResult()))
			{
				LinkInfo->StampClockSyncAdjustment();
			}

			RemoteClockSyncTransaction.Reset();
			SetNextRunASAP();
		}
		else if (ClockSyncer.IsTimeToTune() && !RemoteClockSyncTransaction.IsRequested())
		{
			RemoteClockSyncTransaction.Reset();
			RemoteClockSyncTransaction.SetRequested();
			PrepareClockSyncTuneRequest(RemoteClockSyncTransaction.GetId());
			RequestSendPacket();
		}
		else if (RemoteClockSyncTransaction.IsRequested() && !RemoteClockSyncTransaction.IsFresh(LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME))
		{
			RemoteClockSyncTransaction.Reset();
			SetNextRunASAP();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linked &&
			RemoteClockSyncTransaction.IsRequested())
		{
			RemoteClockSyncTransaction.SetResult(requestId, estimatedError);
			SetNextRunASAP();
		}
	}

	bool OnAckedPacketReceived(ILoLaPacket* receivedPacket)
	{
		if (receivedPacket->GetDataHeader() == LOLA_LINK_HEADER_SHORT_WITH_ACK)
		{
			switch (LinkInfo->GetLinkState())
			{
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				return OnCryptoStartReceived(receivedPacket->GetId(), receivedPacket->GetPayload());
			case LoLaLinkInfo::LinkStateEnum::Linking:
				ATUI_R.array[0] = receivedPacket->GetPayload()[0];
				ATUI_R.array[1] = receivedPacket->GetPayload()[1];
				ATUI_R.array[2] = receivedPacket->GetPayload()[2];
				ATUI_R.array[3] = receivedPacket->GetPayload()[3];

				return OnLinkProtocolReceived(receivedPacket->GetId(), ATUI_R.uint);
			default:
				break;
			}
		}

		return false;
	}
	///

private:
	void PrepareLinkDiscovery()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_LINK_DISCOVERY);

		//TODO: Maybe use data slot for quick reconnect token?
		for (uint8_t i = 1; i < DefinitionShort.GetPayloadSize(); i++)
		{
			OutPacket.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PreparePKCStartRequest()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_REMOTE_PKC_START_REQUEST);
		ATUI_S.uint = LinkInfo->GetLocalId();
		S_ArrayToPayload();
	}

	void PrepareRemoteInfoSync()
	{
		PrepareReportPacket(LOLA_LINK_SUBHEADER_INFO_SYNC_REMOTE);
		OutPacket.GetPayload()[0] = LinkInfo->GetRSSINormalized();
		for (uint8_t i = 1; i < DefinitionReport.GetPayloadSize(); i++)
		{
			OutPacket.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareClockSyncRequest(const uint8_t requestId)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_REQUEST);
		//Rest of Payload is set on OnPreSend.
	}

	void PrepareClockUTCRequest(const uint8_t requestId)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_UTC_REQUEST);
		for (uint8_t i = 1; i < DefinitionShort.GetPayloadSize(); i++)
		{
			OutPacket.GetPayload()[i] = UINT8_MAX; //Padding
		}
	}

	void PrepareClockSyncTuneRequest(const uint8_t requestId)
	{
		PrepareShortPacket(requestId, LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST);
		//Rest of Payload is set on OnPreSend.
	}
};
#endif