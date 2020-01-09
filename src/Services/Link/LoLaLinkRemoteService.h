// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum : uint8_t
	{
		NoSession = 0,
		SearchingForHost = 1,
		ValidatingPartner = 2,
		ProcessingPKE = 3
	} AwaitingLinkState = AwaitingLinkEnum::NoSession;

	LinkRemoteClockSyncer ClockSyncer;


	uint32_t SessionIdTimestamp = 0;

public:
	LoLaLinkRemoteService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: LoLaLinkService(servicesScheduler, driverScheduler, driver, &ClockSyncer)
		, ClockSyncer()
	{
		driver->SetDuplexSlot(false);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

	virtual uint32_t GetSleepDelay()
	{
		return LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD;
	}

	virtual void ResetLinkingState()
	{
		AwaitingLinkState = AwaitingLinkEnum::NoSession;
	}

	void OnPreSend()
	{
		if (OutPacket.GetDataHeader() == DefinitionMulti.Header &&
			OutPacket.GetId() == ClockSyncer.LOLA_LINK_CLOCKSYNC_ESTIMATION_REQUEST)
		{
			// If we are sending an estimation request, we update our synced clock payload as late as possible.
			ATUI_R.uint = (SyncedClock.GetSyncMicros() + LoLaDriver->GetETTMMicros()) % UINT32_MAX;
			OutPacket.GetPayload()[0] = ATUI_R.array[0];
			OutPacket.GetPayload()[1] = ATUI_R.array[1];
			OutPacket.GetPayload()[2] = ATUI_R.array[2];
			OutPacket.GetPayload()[3] = ATUI_R.array[3];
		}
	}

	virtual void OnAckFailed(const uint8_t header, const uint8_t id)
	{
		if (header == DefinitionStartLink.Header &&
			LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver)
		{
			SetNextRunASAP();
		}
	}

	virtual void OnAckOk(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (header == DefinitionStartLink.Header &&
			LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver &&
			id == SessionId)
		{
			LinkingState == LinkingStagesEnum::LinkingDone;
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
			SetNextRunDelay(UINT32_MAX);
			AwaitingLinkState = AwaitingLinkEnum::SearchingForHost;
			SetNextRunASAP();
			break;
		case AwaitingLinkEnum::SearchingForHost:
			// Send our address and protocol code to initiate a link with host.
			if (millis() - SessionIdTimestamp > LOLA_LINK_SERVICE_UNLINK_SEARCH_ID_TIMEOUT)
			{
				RandomizeSessionId();
			}

			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_SEARCH_TIMEOUT)
			{
				UpdateLinkState(LinkStateEnum::AwaitingSleeping);
			}
			else if (GetElapsedMillisSinceLastSent() > SearchingWait)
			{
				PreparePKERequest();
				RequestSendPacket();
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
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.OnDHStarted();
#endif
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
				CryptoEncoder.SetKeyWithSalt(KeyExchanger.GetSharedKey(), SessionSalt);

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
				DebugTask.OnDHDone();
#endif
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
			AwaitingLinkState = AwaitingLinkEnum::NoSession;
			SetNextRunASAP();
			break;
		}
	}

	virtual bool OnPKEResponseReceived(const uint8_t sessionId, const uint32_t sessionSalt, uint8_t* hostPublicKey)
	{
		if (LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::SearchingForHost &&
			SessionId == sessionId &&
			KeyExchanger.SetPartnerPublicKey(hostPublicKey))
		{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.OnDHStarted();
#endif

			SessionSalt = sessionSalt;
			AwaitingLinkState = AwaitingLinkEnum::ValidatingPartner;
			Enable();
			SetNextRunASAP();

			return true;
		}

		return false;
	}

	void OnLinking()
	{
		switch (LinkingState)
		{
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
				if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
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
			// We transition forward when we receive the Ack Ok.
			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkSwitchOver();
				RequestSendPacket(LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD);
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
	void RandomizeSessionId()
	{
		SessionIdTimestamp = millis();
		SessionId = random(UINT8_MAX);
	}

	void PreparePKERequest()
	{
		OutPacket.SetDefinition(&DefinitionPKERequest);
		OutPacket.SetId(SessionId);

		ATUI_S.uint = ProtocolVersion.GetCode();
		OutPacket.GetPayload()[0] = ATUI_S.array[0];
		OutPacket.GetPayload()[1] = ATUI_S.array[1];
		OutPacket.GetPayload()[2] = ATUI_S.array[2];
		OutPacket.GetPayload()[3] = ATUI_S.array[3];

		for (uint8_t i = 0; i < KeyExchanger.KEY_CURVE_SIZE; i++)
		{
			OutPacket.GetPayload()[i + sizeof(uint32_t)] = KeyExchanger.GetPublicKeyCompressed()[i];
		}
	}

	void PrepareLinkSwitchOver()
	{
		OutPacket.SetDefinition(&DefinitionStartLink);
		OutPacket.SetId(SessionId);
	}
};
#endif