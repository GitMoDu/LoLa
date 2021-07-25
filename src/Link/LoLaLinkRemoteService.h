// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum
	{
		NoSession = 0,
		SearchingForHost = 1,
		ValidatingPartner = 2,
		ProcessingPKE = 3
	} AwaitingLinkState = AwaitingLinkEnum::NoSession;

	LinkRemoteClockSyncer ClockSyncer;

	uint32_t SessionIdTimestamp = 0;
	uint32_t SearchingLastChannelUpdateTimestamp = 0;

public:
	LoLaLinkRemoteService(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaLinkService(scheduler, driver, &ClockSyncer)
		, ClockSyncer(&driver->SyncedClock)
	{
		driver->SetDuplexSlot(false);
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

protected:

	virtual uint32_t GetSleepDelay()
	{
		return LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD;
	}

	virtual void ResetLinkingState()
	{
		AwaitingLinkState = AwaitingLinkEnum::NoSession;
	}

	virtual void OnPreSend(const uint8_t header)
	{
		if (header == LinkPackets::CLOCK_SYNC &&
			GetOutPayload()[0] == ClockSyncer.LOLA_LINK_CLOCKSYNC_ESTIMATION_REQUEST)
		{
			// If we are sending an estimation request, we update our synced clock payload as late as possible.
			ArrayToUint32 ATUI;
			ATUI.uint = (LoLaDriver->SyncedClock.GetSyncMicros() + LoLaDriver->DriverSettings->ETTM);
			GetOutPayload()[1] = ATUI.array[0];
			GetOutPayload()[2] = ATUI.array[1];
			GetOutPayload()[3] = ATUI.array[2];
			GetOutPayload()[4] = ATUI.array[3];
		}
	}

	virtual void OnAckFailed(const uint8_t header)
	{
		if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ChallengeStage &&
			header == LinkPackets::CHALLENGE)
		{
			SetNextRunASAP();
		}
		else if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver &&
			header == LinkPackets::LINK_START)
		{
			SetNextRunASAP();
		}
	}

	virtual void OnAckOk(const uint8_t header)
	{
		if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ChallengeStage &&
			header == LinkPackets::CHALLENGE)
		{
			LinkingState = LinkingStagesEnum::ClockSyncStage;
			SetNextRunASAP();
		}
		else if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver &&
			header == LinkPackets::LINK_START)
		{
			LinkingState = LinkingStagesEnum::LinkingDone;
			SetNextRunASAP();
		}
	}

	void OnAwaitingLink()
	{
		uint32_t SearchingWait;

		if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_SEARCH_EASE_TIMEOUT)
		{
			SearchingWait = LOLA_LINK_SERVICE_UNLINK_SEARCH_PERIOD_MAX;
		}
		else
		{
			SearchingWait = LOLA_LINK_SERVICE_UNLINK_SEARCH_PERIOD_MIN;
		}

		switch (AwaitingLinkState)
		{
		case AwaitingLinkEnum::NoSession:
			KeyExchanger.ClearPartner();
			AwaitingLinkState = AwaitingLinkEnum::SearchingForHost;
			Serial.println(F("OnAwaitingLink SearchingForHost"));
			SetNextRunASAP();
			break;
		case AwaitingLinkEnum::SearchingForHost:
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_SEARCH_TIMEOUT)
			{
				UpdateLinkState(LinkStateEnum::AwaitingSleeping);
			}

			// Until we get a reply from a host, we don't know which channel to search in.
			//if (millis() - SearchingLastChannelUpdateTimestamp > 2000)
			//	//if (millis() - SearchingLastChannelUpdateTimestamp > LOLA_LINK_SERVICE_UNLINK_REMOTE_CHANNEL_HOP_PERIOD)
			//{
			//	SearchingLastChannelUpdateTimestamp = millis();
			//	LoLaDriver->ChannelManager.NextRandomChannel();
			//}

			// Send our public id and protocol code to initiate a link with host.
			if (!IsSending() && GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_SEARCH_RETRY_PERIOD)
			{
				SendPKERequest();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::ValidatingPartner:
			if (!KeyExchanger.GotPartnerPublicKey())
			{
				AwaitingLinkState = AwaitingLinkEnum::NoSession;
				SetNextRunASAP();
				return;
			}

			//TODO: Filter address from known list.
			if (!true)
			{
				AwaitingLinkState = AwaitingLinkEnum::NoSession;
				SetNextRunASAP();
				return;
			}

			ProcessingDHState = ProcessingDHEnum::GeneratingSecretKey;
			AwaitingLinkState = AwaitingLinkEnum::ProcessingPKE;
			SetNextRunASAP();
			break;
		case AwaitingLinkEnum::ProcessingPKE:
			switch (ProcessingDHState)
			{
			case ProcessingDHEnum::GeneratingSecretKey:
				KeyExchanger.GenerateSharedKey();

				ProcessingDHState = ProcessingDHEnum::ExpandingKey;
				SetNextRunASAP();
				break;
			case ProcessingDHEnum::ExpandingKey:
				// All set to start linking.
				UpdateLinkState(LinkStateEnum::Linking);
				break;
			default:
				AwaitingLinkState = AwaitingLinkEnum::NoSession;
				SetNextRunASAP();
				break;
			}
			break;
		default:
			Serial.println(F("OnAwaitingLink NoSession"));

			AwaitingLinkState = AwaitingLinkEnum::NoSession;
			SetNextRunASAP();
			break;
		}
	}

	virtual bool OnPKEResponseReceived(const uint32_t sessionId, uint8_t* hostPublicKey)
	{
		if (LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::SearchingForHost &&
			KeyExchanger.SetPartnerPublicKey(hostPublicKey))
		{
			LinkInfo->SetSessionId(sessionId);

			AwaitingLinkState = AwaitingLinkEnum::ValidatingPartner;
			ClearSendRequest();

			return true;
		}

		return false;
	}

	void OnLinking()
	{
		switch (LinkingState)
		{
		case LinkingStagesEnum::ChallengeStage:
			if (GetElapsedSinceLastSent() > ILOLA_DUPLEX_PERIOD_MILLIS / 2)
			{
				SendChallengeRequest();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case LinkingStagesEnum::ClockSyncStage:
			if (ClockSyncer.HasSync())
			{
				LinkingState = LinkingStagesEnum::LinkProtocolSwitchOver;
				SetNextRunASAP();
			}
			else if (ProcessClockSync())
			{
				// Sending packet.
			}
			else
			{
				if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
				else
				{
					ClockSyncer.RequestSync();
				}
			}
			break;
		case LinkingStagesEnum::LinkProtocolSwitchOver:
			// We transition forward only when we receive the Ack Ok.
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				SendLinkSwitchOver();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case LinkingStagesEnum::LinkingDone:
			// All linking stages complete, we have a link.
			UpdateLinkState(LinkStateEnum::Linked);
			break;
		default:
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			break;
		}
	}

	void OnKeepingLink()
	{
		if (ClockSyncer.Update())
		{
			// Sending packet.
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

private:
	void SendPKERequest()
	{
		ArrayToUint32 ATUI;

		ATUI.uint = LinkInfo->LinkProtocolVersion;
		GetOutPayload()[0] = ATUI.array[0];
		GetOutPayload()[1] = ATUI.array[1];
		GetOutPayload()[2] = ATUI.array[2];
		GetOutPayload()[3] = ATUI.array[3];

		uint8_t* LocalPublicKeyCompressed = KeyExchanger.GetPublicKeyCompressed();

		for (uint8_t i = 0; i < KeyExchanger.KEY_MAX_SIZE; i++)
		{
			GetOutPayload()[sizeof(uint32_t) + i] = LocalPublicKeyCompressed[i];
		}

		RequestSendPacket(LinkPackets::PKE_REQUEST);
	}

	void SendChallengeRequest()
	{
		ArrayToUint32 ATUI;
		ATUI.uint = LinkInfo->GetSessionIdSignature();

		GetOutPayload()[0] = ATUI.array[0];
		GetOutPayload()[1] = ATUI.array[1];
		GetOutPayload()[2] = ATUI.array[2];
		GetOutPayload()[3] = ATUI.array[3];

		RequestSendPacket(LinkPackets::CHALLENGE);
	}

	void SendLinkSwitchOver()
	{
		RequestSendPacket(LinkPackets::LINK_START);
	}
};
#endif