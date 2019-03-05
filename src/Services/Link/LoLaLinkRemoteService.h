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

	LinkRemoteClockSyncer ClockSyncer;
	ClockSyncRequestTransaction RemoteClockSyncTransaction;

	ChallengeReplyTransaction RemoteChallengeTransaction;

public:
	LoLaLinkRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &RemoteClockSyncTransaction;
		ChallengeTransaction = &RemoteChallengeTransaction;
#ifdef USE_TIME_SLOT
		loLa->SetDuplexSlot(true);
#endif
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

	//Remote version, ParnerMAC is the Host's MAC.
	//void SetBaseSeed()
	//{
	//	GetLoLa()->GetCryptoSeed()->SetBaseSeed(LinkInfo->GetPartnerMACHash(), LinkInfo->GetLocalMACHash(), LinkInfo->GetSessionId());
	//}

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			KeyExchanger.GenerateNewKeyPair();
			ClearSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			RemoteClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnAwaitingLink()
	{
		if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_REMOTE_MAX_BEFORE_SLEEP)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
		}
		else
		{
			switch (LinkingState)
			{
			case AwaitingLinkEnum::SearchingForHost:
				if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_REMOTE_SEARCH_PERIOD)
				{
					//Send an Hello to wake up potential hosts.
					PrepareLinkDiscovery();
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				break;
			case AwaitingLinkEnum::ValidatingPartner:
				//TODO: Filter accepted hosts by MAC hash.
				if (true)
				{
					GetLoLa()->GetCryptoEncoder()->SetIvData(LinkInfo->GetSessionId(),
						LinkInfo->GetPartnerMACHash(), LinkInfo->GetLocalMACHash());

					LinkingStart = millis();//Reset local timeout.
					ResetLastSentTimeStamp();
					SetLinkingState(AwaitingLinkEnum::AwaitingHostPublicKey);
				}
				else
				{
					LinkInfo->Reset();
					SetLinkingState(AwaitingLinkEnum::SearchingForHost);
				}
				break;
			case AwaitingLinkEnum::AwaitingHostPublicKey:
				if (millis() - LinkingStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
				{
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
					SetNextRunASAP();
				}
				else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
				{
					PreparePKCStartRequest();
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				break;
			case AwaitingLinkEnum::ProcessingSharedKey:
				//TODO: Solve key size issue
				//GetLoLa()->GetCryptoEncoder()->SetIv() //TODO: Set Iv from known entropy + session id.
				if (GetLoLa()->GetCryptoEncoder()->SetSecretKey(KeyExchanger.GetSharedKeyPointer(), 16) &&
					GetLoLa()->GetCryptoEncoder()->SetAuthData(nullptr, 0) &&
					GetLoLa()->GetCryptoEncoder()->IsReadyForUse())
				{
#ifdef DEBUG_LOLA
					uint8_t pk[21];

					for (uint8_t i = 0; i < 21; i++)
					{
						pk[i] = 0;
					}

					Serial.print(F("Local Public Key\n\t|"));
					KeyExchanger.GetPublicKeyCompressed(pk);
					for (uint8_t i = 0; i < 21; i++)
					{
						Serial.print(pk[i]);
						Serial.print('|');
					}
					Serial.println();

					for (uint8_t i = 0; i < 21; i++)
					{
						pk[i] = 0;
					}

					Serial.print(F("Partner Public Key\n\t|"));
					KeyExchanger.GetPartnerPublicKeyCompressed(pk);

					for (uint8_t i = 0; i < 21; i++)
					{
						Serial.print(pk[i]);
						Serial.print('|');
					}
					Serial.println();

					for (uint8_t i = 0; i < 21; i++)
					{
						pk[i] = 0;
					}

					Serial.print(F("Shared Key\n\t|"));
					for (uint8_t i = 0; i < 20; i++)
					{
						Serial.print(KeyExchanger.GetSharedKeyPointer()[i]);
						Serial.print('|');
					}
					Serial.println();
#endif
					LinkingStart = millis();//Reset local timeout.
					ResetLastSentTimeStamp();
					SetLinkingState(AwaitingLinkEnum::SendingPublicKey);
				}
				else
				{
					Serial.println("Nope!");
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
					SetNextRunASAP();
				}
				break;
			case AwaitingLinkEnum::SendingPublicKey:
				if (millis() - LinkingStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
				{
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
					SetNextRunASAP();
				}
				else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD)
				{
					PreparePublicKeyPacket(LOLA_LINK_SUBHEADER_REMOTE_PUBLIC_KEY);
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				break;
			case AwaitingLinkEnum::LinkingSwitchOver:
				//All set to start linking.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
				break;
			default:
				break;
			}
		}
	}
	void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostMACHash)
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
					LinkInfo->SetPartnerMACHash(hostMACHash);
					SetLinkingState(AwaitingLinkEnum::ValidatingPartner);
				}
				break;
			case AwaitingLinkEnum::ValidatingPartner:
			case AwaitingLinkEnum::AwaitingHostPublicKey:
				//In case we have a pending link and our target host has a new session.
				//Note: this is an easy target for denial of service.
				if (LinkInfo->HasSession() &&
					LinkInfo->PartnerMACHashMatches(hostMACHash) &&
					LinkInfo->SetSessionId(sessionId))
				{
					LinkInfo->SetPartnerMACHash(hostMACHash);
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

	bool OnCryptoStartReceived(const uint8_t sessionId, uint8_t* encodedMACHashArray)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::SendingPublicKey &&
				LinkInfo->HasSessionId() &&
				LinkInfo->GetSessionId() == sessionId)
			{
				if (!GetLoLa()->GetCryptoEncoder()->Decode(encodedMACHashArray, sizeof(uint32_t)))
				{
#ifdef DEBUG_LOLA
					Serial.println("Failed to decode");
#endif
				}
				if (GetLoLa()->GetCryptoEncoder()->Decode(encodedMACHashArray, sizeof(uint32_t), ATUI_R.array) &&
					LinkInfo->PartnerMACHashMatches(ATUI_R.uint))
				{
					Serial.println("Crypto start accepted");
					SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);


					return true;
				}
				else
				{
					Serial.println("Crypto start rejected");
					Serial.println("Decoded Partner Id: |");
					for (uint8_t i = 0; i < sizeof(uint32_t); i++)
					{
						Serial.print(ATUI_R.array[i]);
						Serial.print('|');
					}
					Serial.print("\n\t ");
					Serial.print(ATUI_R.uint);
					Serial.print(" vs ");
					Serial.println(LinkInfo->GetPartnerMACHash());
				}
			}




			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			//This should never happen, as packets are encrypted at this point.
			break;
		default:
			break;
		}
	}

	//void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader)
	//{
	//	switch (LinkInfo->GetLinkState())
	//	{
	//	case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
	//		if (subHeader == LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER &&
	//			LinkingState == AwaitingLinkEnum::LinkingSwitchOver)
	//		{
	//!LinkInfo->HasSession() ||
	//	!KeyExchanger.HasSharedKey() ||
	//			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
	//		}
	//		break;
	//	case LoLaLinkInfo::LinkStateEnum::Linking:
	//		switch (LinkingState)
	//		{
	//		case LinkingStagesEnum::ClockSyncStage:
	//			//If we break here, we need to receive two of the same protocol packet.
	//		case LinkingStagesEnum::ClockSyncSwitchOver:
	//			if (subHeader == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER &&
	//				requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
	//			{
	//				ClockSyncer.SetSynced();
	//				SetLinkingState(LinkingStagesEnum::ChallengeStage);
	//			}
	//			break;
	//		case LinkingStagesEnum::ChallengeStage:
	//			if (RemoteChallengeTransaction.OnReplyAccepted(requestId))
	//			{
	//				SetLinkingState(LinkingStagesEnum::ChallengeSwitchOver);
	//			}
	//			else
	//			{
	//				break;
	//			}
	//			//If we break here, we need to receive two of the same protocol packet.
	//		case LinkingStagesEnum::ChallengeSwitchOver:
	//			if (subHeader == LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER &&
	//				RemoteChallengeTransaction.IsComplete())
	//			{
	//				SetLinkingState(LinkingStagesEnum::InfoSyncStage);
	//			}
	//			break;
	//		case LinkingStagesEnum::InfoSyncStage:
	//			if (subHeader == LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE)
	//			{
	//				RemoteInfoTransaction.OnAdvanceRequestReceived(requestId);
	//				SetNextRunASAP();
	//			}
	//			break;
	//		case LinkingStagesEnum::LinkProtocolSwitchOver:
	//			if (subHeader == LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER &&
	//				LinkInfo->GetSessionId() == requestId)
	//			{
	//				SetLinkingState(LinkingStagesEnum::AllConnectingStagesDone);
	//			}
	//			break;
	//		default:
	//			break;
	//		}
	//	default:
	//		break;
	//	}
	//}



	//Possibly CPU intensive task.
	//void OnChallenging()
	//{
	//	if (RemoteChallengeTransaction.IsReplyReady() && GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		PrepareChallengeReply();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}

	//void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token)
	//{
	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::ChallengeStage)
	//	{
	//		RemoteChallengeTransaction.Clear();
	//		RemoteChallengeTransaction.OnRequest(requestId, token);
	//		SetNextRunASAP();
	//	}
	//}

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
		else if (ClockSyncer.IsTimeToTune())
		{
			if (!RemoteClockSyncTransaction.IsRequested())
			{
				RemoteClockSyncTransaction.Reset();
				PrepareClockSyncTuneRequest(RemoteClockSyncTransaction.GetId());
				RemoteClockSyncTransaction.SetRequested();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	///Clock Sync
	//void OnClockSync()
	//{
	//	if (RemoteClockSyncTransaction.IsResultWaiting())
	//	{
	//		ClockSyncer.OnEstimationErrorReceived(RemoteClockSyncTransaction.GetResult());
	//		RemoteClockSyncTransaction.Reset();
	//		SetNextRunASAP();
	//	}
	//	else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		RemoteClockSyncTransaction.Reset();
	//		PrepareClockSyncRequest(RemoteClockSyncTransaction.GetId());
	//		RemoteClockSyncTransaction.SetRequested();//TODO: Only set requested on OnSendOk, to reduce the possible DOS window.
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}

	//void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	//{
	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::ClockSyncStage &&
	//		RemoteClockSyncTransaction.IsRequested() &&
	//		RemoteClockSyncTransaction.GetId() == requestId)
	//	{
	//		RemoteClockSyncTransaction.SetResult(estimatedError);
	//		SetNextRunASAP();
	//	}
	//}

	void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linked &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.GetId() == requestId)
		{
			RemoteClockSyncTransaction.SetResult(estimatedError);
			SetNextRunASAP();
		}
	}
	///



	//void OnPreSend()
	//{
	//	if (PacketHolder.GetDataHeader() == DefinitionLinked.GetHeader() &&
	//		(PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_REQUEST ||
	//			PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST))
	//	{
	//		//If we are sending a clock sync request, we update our synced clock payload as late as possible.
	//		ATUI_S.uint = MillisSync();
	//		ArrayToPayload();
	//	}
	//}

	/////Info Sync
	//bool OnInfoSync()
	//{
	//	//First move is done by host.
	//	//We wait until we get 
	//	if (LatencyMeter.GetSampleCount() >= LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES)
	//	{
	//		LinkInfo->SetRTT(LatencyMeter.GetAverageLatency());

	//		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//		{
	//			PrepareLinkInfoSyncUpdate(LatencyMeter.GetAverageLatency(), LinkInfo->GetRSSINormalized());
	//			RequestSendPacket(true);
	//		}
	//		else
	//		{
	//			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		}
	//	}
	//	else
	//	{
	//		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//		{
	//			PreparePing();
	//			RequestSendPacket(true);
	//		}
	//		else
	//		{
	//			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		}
	//	}

	//	return LinkInfo->HasPartnerRSSI();
	//	switch (RemoteInfoTransaction.GetStage())
	//	{
	//	case InfoSyncTransaction::StageEnum::StageStart:
	//		//We wait in this state until we received the first Stage update.
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		break;
	//	case InfoSyncTransaction::StageEnum::StageHostRTT:
	//		//We have Host RTT, let's send our RSSI to let him know to move on.
	//		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//		{
	//			PrepareLinkInfoSyncUpdate(InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI, LinkInfo->GetRSSINormalized());
	//			RequestSendPacket(true);
	//		}
	//		else
	//		{
	//			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		}
	//		break;
	//	case InfoSyncTransaction::StageEnum::StageHostRSSI:
	//		//Info sync is done, but host doesn't know that.
	//		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//		{
	//			PrepareLinkInfoSyncAdvanceRequest(InfoSyncTransaction::ContentIdEnum::ContentHostRSSI);
	//			RequestSendPacket(true);
	//		}
	//		else
	//		{
	//			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		}
	//		//if (LinkInfo->HasPartnerRSSI())

	//		break;
	//	case InfoSyncTransaction::StageEnum::StageRemoteRSSI:
	//		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//		{
	//			PrepareLinkInfoSyncUpdate(InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI, LinkInfo->GetRSSINormalized());
	//			RequestSendPacket(true);
	//		}
	//		else
	//		{
	//			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//		}
	//		break;
	//	case InfoSyncTransaction::StageEnum::StagesDone:
	//		SetNextRunASAP();
	//		break;
	//	default:
	//		RemoteInfoTransaction.Clear();
	//		break;
	//	}
	//}

	//void OnLinkInfoSyncReceived(const uint16_t rtt, const uint8_t rssi)
	//{
	//	if (RemoteInfoTransaction.OnUpdateReceived(contentId))
	//	{
	//		//We don't check for Stage here because OnUpdateReceived already validated Stage.
	//		switch (contentId)
	//		{
	//		case InfoSyncTransaction::ContentIdEnum::ContentHostRTT:
	//			LinkInfo->SetRTT((uint16_t)content);
	//			break;
	//		case InfoSyncTransaction::ContentIdEnum::ContentHostRSSI:
	//			LinkInfo->SetPartnerRSSINormalized((uint8_t)content);
	//			break;
	//		default:
	//			break;
	//		}
	//		SetNextRunASAP();
	//	}
	//}

	bool OnAckedPacketReceived(ILoLaPacket* receivedPacket)
	{
		switch (receivedPacket->GetDataHeader())
		{
		case LOLA_LINK_HEADER_SHORT_WITH_ACK:
			//To host.
			return OnCryptoStartReceived(receivedPacket->GetId(), receivedPacket->GetPayload());
		default:
			break;
		}

		return false;
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::Linked:
			//Ping Ack, ignore.
			break;
		default:
			break;
		}
	}
	///

private:
	void PrepareLinkDiscovery()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_LINK_DISCOVERY);

		//TODO: Maybe use data slot for quick reconnect token?

		PacketHolder.GetPayload()[1] = UINT8_MAX; //Padding
		PacketHolder.GetPayload()[2] = UINT8_MAX; //Padding
		PacketHolder.GetPayload()[3] = UINT8_MAX; //Padding
		PacketHolder.GetPayload()[4] = UINT8_MAX; //Padding
	}

	void PreparePKCStartRequest()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_REMOTE_PKC_START_REQUEST);
		ATUI_S.uint = LinkInfo->GetLocalMACHash();
		S_ArrayToPayload();
	}





	//void PrepareEncodedCryptoStart()
	//{
	//	PacketHolder.SetDefinition(&DefinitionUnlinkedWithAck);

	//	PacketHolder.SetId(LOLA_LINK_SUBHEADER_REMOTE_CRYPTO_START_ENCODED);

	//	//TODO: Sign our public key with our private key.
	//	//TODO: Figure out how to send >40 bytes.

	//	//TODO: Remove after signature is implemented.
	//	PacketHolder.GetPayload()[0] = LinkInfo->GetSessionId();

	//	//TODO: Encode payload with shared key.
	//}
};
#endif