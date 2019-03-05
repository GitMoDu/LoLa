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

	LinkHostClockSyncer ClockSyncer;
	ClockSyncResponseTransaction HostClockSyncTransaction;

	ChallengeRequestTransaction HostChallengeTransaction;

	//Latency measurement.
	LoLaLinkLatencyMeter<LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES> LatencyMeter;

	//Session timing.
	uint32_t SessionLastStarted = ILOLA_INVALID_MILLIS;

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &HostClockSyncTransaction;
		ChallengeTransaction = &HostChallengeTransaction;
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
	uint32_t SharedKeyTime;
#endif // DEBUG_LOLA

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (!NewSession())
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Unable to start session."));
#endif
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			HostChallengeTransaction.NewRequest();
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			ClockSyncer.SetReadyForEstimation();
			HostClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	//Host version, PartnerMAC is the Remote's MAC.
	//void SetBaseSeed()
	//{
	//	GetLoLa()->GetCryptoSeed()->SetBaseSeed(LinkInfo->GetLocalMACHash(), LinkInfo->GetPartnerMACHash(), LinkInfo->GetSessionId());
	//}

	void OnClearSession()
	{
		LatencyMeter.Reset();
		SessionLastStarted = ILOLA_INVALID_MILLIS;
	}

	void OnAwaitingLink()
	{
		if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
		}
		else switch (LinkingState)
		{
		case AwaitingLinkEnum::BroadcastingOpenSession:
			if (SessionLastStarted == ILOLA_INVALID_MILLIS || millis() - SessionLastStarted > LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME)
			{
				NewSession();
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
			//TODO: Filter accepted MACHash from known list.
			if (true)
			{
				GetLoLa()->GetCryptoEncoder()->SetIvData(LinkInfo->GetSessionId(),
					LinkInfo->GetLocalMACHash(), LinkInfo->GetPartnerMACHash());

				LinkingStart = millis();//Reset local timeout.
				ResetLastSentTimeStamp();
				SetLinkingState(AwaitingLinkEnum::SendingPublicKey);
			}
			else
			{
				LinkInfo->ClearPartnerMAC();
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			break;
		case AwaitingLinkEnum::SendingPublicKey:
			if (millis() - LinkingStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
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
			//TODO: Use authorization data?
			if (KeyExchanger.GenerateSharedKey() &&
				GetLoLa()->GetCryptoEncoder()->SetSecretKey(KeyExchanger.GetSharedKeyPointer(), 16) &&
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
				SetLinkingState(AwaitingLinkEnum::GotSharedKey);
			}
			else
			{
				Serial.println("Nope!");
				SetLinkingState(AwaitingLinkEnum::BroadcastingOpenSession);
			}
			break;
		case AwaitingLinkEnum::GotSharedKey:
			if (millis() - LinkingStart > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				if (PrepareCryptoStartRequest())
				{
					Serial.println("Send Crypto Start Request");
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_SERVICE_LONG_SLEEP_PERIOD_MILLIS);
				}

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
			SetLinkingState(0);
			SetNextRunASAP();
			break;
		}
	}

	void OnLinkDiscoveryReceived()
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remoteMACHash)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::BroadcastingOpenSession &&
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId)
		{
			LinkInfo->SetPartnerMACHash(remoteMACHash);
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
			else
			{
				Serial.println("Partner Key Rejected");
			}
		}
	}



	//void OnCryptoStartAckReceived(const uint8_t subHeader)
	//{
	//}
	/*void OnLinkPacketAckReceived(const uint8_t requestId)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::LinkingSwitchOver &&
				LinkInfo->GetSessionId() == requestId &&
				LinkInfo->HasPartnerMACHash())
			{

			!LinkInfo->HasSession() ||
				!KeyExchanger.HasSharedKey() ||
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			switch (LinkingState)
			{
			case LinkingStagesEnum::ClockSyncSwitchOver:
				if (requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
				{
					SetLinkingState(LinkingStagesEnum::ChallengeStage);
				}
				break;
			case LinkingStagesEnum::ChallengeSwitchOver:
				if (requestId == ChallengeTransaction->GetTransactionId())
				{
					SetLinkingState(LinkingStagesEnum::InfoSyncStage);
				}
				break;
			case LinkingStagesEnum::InfoSyncStage:
				HostInfoTransaction.OnRequestAckReceived(requestId);
				SetNextRunASAP();
				break;
			case LinkingStagesEnum::LinkProtocolSwitchOver:
				if (LinkInfo->GetSessionId() == requestId)
				{
					SetLinkingState(LinkingStagesEnum::AllConnectingStagesDone);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}*/

	//void OnLinking()
	//{
	//	switch (LinkingState)
	//	{
	//	case LinkingStagesEnum::ChallengeStage:
	//		if (false)
	//		{
	//			SetLinkingState(LinkingStagesEnum::ClockSyncStage);
	//		}
	//		break;
	//	case LinkingStagesEnum::InfoSyncStage:
	//		if (false)
	//		{
	//			SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
	//		}
	//		break;
	//	case LinkingStagesEnum::ClockSyncStage:
	//		if (false)
	//		{
	//			SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
	//		}
	//		break;
	//	case LinkingStagesEnum::LinkProtocolSwitchOver:
	//		//We transition forward when we receive the exppected packet/ack.
	//		OnLinkProtocolSwitchOver();
	//		break;
	//	case LinkingStagesEnum::LinkingDone:
	//		//All connecting stages complete, we have a link.
	//		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linked);
	//		break;
	//	default:
	//		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
	//		SetNextRunASAP();
	//		break;
	//	}
	//}



	///Clock Sync.
	//void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	//{
	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::ClockSyncStage)
	//	{
	//		HostClockSyncTransaction.SetResult(requestId,
	//			(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
	//		SetNextRunASAP();
	//	}
	//}

	//void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	//{
	//	if (LinkInfo->HasLink())
	//	{
	//		HostClockSyncTransaction.SetResult(requestId,
	//			(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
	//		SetNextRunASAP();
	//	}
	//}

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

	//void OnClockSync()
	//{
	//	if (ClockSyncer.IsSynced())
	//	{
	//		SetNextRunASAP();
	//	}
	//	else if (HostClockSyncTransaction.IsResultReady())
	//	{
	//		ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

	//		PrepareClockSyncResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
	//		HostClockSyncTransaction.Reset();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}

	//void OnClockSyncSwitchOver()
	//{
	//	if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		PrepareClockSyncSwitchOver();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}
	/////

	/////Challenge. Possibly CPU intensive task.
	//void OnChallenging()
	//{
	//	if (HostChallengeTransaction.IsStale())
	//	{
	//		HostChallengeTransaction.NewRequest();
	//	}

	//	if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		PrepareChallengeRequest();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}

	//void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token)
	//{
	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::ChallengeStage)
	//	{
	//		HostChallengeTransaction.OnReply(requestId, token);
	//		SetNextRunASAP();
	//	}
	//}

	//void OnChallengeSwitchOver()
	//{
	//	if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		PrepareChallengeSwitchOver();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}
	/////

	/////Info Sync
	//bool OnInfoSync()
	//{
	//	//First move is done by host.
	//	//If we don't have enough latency samples, we make more.
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
	//}

	//void OnLinkInfoSyncUpdateReceived(const uint16_t rtt, const uint8_t rssi)
	//{

	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::InfoSyncStage)
	//	{
	//		if (rtt == LinkInfo->GetRTT())
	//		{
	//			LinkInfo->SetPartnerRSSINormalized(rssi);
	//			HostInfoTransaction.Advance();
	//			SetNextRunASAP();
	//		}
	//		else
	//		{
	//			Serial.println("RTT mismatch");
	//		}
	//	}
	//}

	//void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader)
	//{
	//	if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
	//		LinkingState == LinkingStagesEnum::InfoSyncStage &&
	//		subHeader == LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE)
	//	{
	//		HostInfoTransaction.OnAdvanceRequestReceived(requestId);
	//		SetNextRunASAP();
	//	}
	//}
	///

	/////Protocol promotion to connection!
	//void OnLinkProtocolSwitchOver()
	//{
	//	if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
	//	{
	//		PrepareLinkProtocolSwitchOver();
	//		RequestSendPacket(true);
	//	}
	//	else
	//	{
	//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	//	}
	//}
	/////


	void OnPreSend()
	{
		if (!LinkInfo->HasLink() && PacketHolder.GetDefinition()->HasACK())
		{
			//Piggy back on any link with ack packets to measure latency.
			LatencyMeter.OnAckPacketSent(PacketHolder.GetId());
		}
	}


	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			LatencyMeter.OnAckReceived(id);
			if (header == LOLA_LINK_HEADER_SHORT &&
				LinkingState == AwaitingLinkEnum::GotSharedKey &&
				LinkInfo->GetSessionId() == id)
			{
				SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			LatencyMeter.OnAckReceived(id);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			//Ping Ack, ignore.
			break;
		default:
			break;
		}
	}

	//void OnAckReceived(const uint8_t header, const uint8_t id)
	//{
	//	if (!LinkInfo->HasLink())
	//	{
	//		//Catch and store acked packets' round trip time.
	//		LatencyMeter.OnAckReceived(id);
	//	}

	//	switch (header)
	//	{
	//	case PACKET_DEFINITION_UNLINKED_LONG_HEADER:
	//		OnCryptoStartAckReceived(id);
	//		break;
	//	default:
	//		break;
	//	}
	//}


private:
	bool NewSession()
	{
		ClearSession();

#ifdef DEBUG_LOLA
		SharedKeyTime = micros();
#endif

		if (!KeyExchanger.GenerateNewKeyPair())
		{
			return false;
		}

		if (!LinkInfo->SetSessionId((uint8_t)random(1, (UINT8_MAX - 1))))
		{
			return false;
		}

#ifdef DEBUG_LOLA
		SharedKeyTime = micros() - SharedKeyTime;
		Serial.print(F("New keys took "));
		Serial.print(SharedKeyTime);
		Serial.println(F(" us to generate."));
#endif

		SessionLastStarted = millis();

		return true;
	}

	void PrepareIdBroadcast()
	{
		PrepareShortPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST);
		ATUI_S.uint = LinkInfo->GetLocalMACHash();
		S_ArrayToPayload();
	}

	bool PrepareCryptoStartRequest()
	{
		PrepareShortPacketWithAck(LinkInfo->GetSessionId());
		ATUI_S.uint = LinkInfo->GetPartnerMACHash();

		if (!GetLoLa()->GetCryptoEncoder()->Encode(PacketHolder.GetPayload(), sizeof(uint32_t)))
		{
#ifdef DEBUG_LOLA
			Serial.println("Failed to encode");
#endif
			return false;
		}

#ifdef DEBUG_LOLA
		if (!GetLoLa()->GetCryptoEncoder()->Decode(PacketHolder.GetPayload(), sizeof(uint32_t)))
		{
			Serial.println("Failed to decode");
		}

		ArrayToR_Array(PacketHolder.GetPayload());
		if (!LinkInfo->PartnerMACHashMatches(ATUI_R.uint))
		{
			Serial.println("Failed decoded match");
			Serial.print('|');
			for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			{
				Serial.print(ATUI_R.array[i]);
				Serial.print('|');
			}
			Serial.print(" vs |");
			ATUI_R.uint = LinkInfo->GetPartnerMACHash();
			for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			{
				Serial.print(ATUI_R.array[i]);
				Serial.print('|');
			}
			Serial.println();

			return false;
		}
#endif

		return true;
	}
};
#endif