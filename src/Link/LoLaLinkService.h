// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h


#include <Link\AbstractLinkService.h>


class LoLaLinkService : public AbstractLinkService
{
private:
	// Link report tracking.
	bool ReportPending = false;

protected:
	// Shared Sub state helpers.
	enum LinkingStagesEnum
	{
		ChallengeStage = 0,
		ClockSyncStage = 1,
		LinkProtocolSwitchOver = 2,
		LinkingDone = 3
	} LinkingState = LinkingStagesEnum::ChallengeStage;

	enum ProcessingDHEnum
	{
		GeneratingSecretKey = 0,
		ExpandingKey = 1
	} ProcessingDHState = ProcessingDHEnum::GeneratingSecretKey;


protected:

	// Internal housekeeping.
	virtual void OnStateChanged() {}
	virtual uint32_t GetSleepDelay() { return LOLA_LINK_SERVICE_CHECK_PERIOD; }
	virtual void ResetLinkingState() {}

	// Host packet handling.
	virtual void OnPKERequestReceived(const uint32_t protocolCode, uint8_t* remotePublicKey) {}
	virtual bool OnChallengeRequestReceived(const uint32_t signedSessionId) { return false; }
	virtual bool OnLinkSwitchOverReceived() { return false; }

	// Remote packet handling.
	virtual bool OnPKEResponseReceived(const uint32_t sessionSalt, uint8_t* hostPublicKey) { return false; }

	// Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual void OnAwaitingLink() {}
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }

public:
	LoLaLinkService(Scheduler* scheduler, LoLaPacketDriver* driver, LoLaLinkClockSyncer* clockSyncer)
		: AbstractLinkService(scheduler, driver, clockSyncer)
	{
	}

	virtual bool Setup()
	{
		if (AbstractLinkService::Setup())
		{
			ClearSession();
			ResetStateStartTime();

			return true;

		}

		return false;
	}

	void Start()
	{
		LoLaDriver->Start();
		UpdateLinkState(LinkStateEnum::AwaitingLink);
	}

	void Stop()
	{
		UpdateLinkState(LinkStateEnum::Disabled);
	}

	virtual bool OnPacketReceived(const uint8_t header, const uint32_t timestamp, uint8_t* payload)
	{
		ArrayToUint32 ATUI;

		// Switch the packet to the appropriate method.
		switch (header)
		{
		case LinkPackets::PKE_REQUEST:
			// To Host. Unencrypted packet.
			ATUI.array[0] = payload[0];
			ATUI.array[1] = payload[1];
			ATUI.array[2] = payload[2];
			ATUI.array[3] = payload[3];
			OnPKERequestReceived(ATUI.uint, &payload[sizeof(uint32_t)]);
			break;
		case LinkPackets::PKE_RESPONSE:
			// To remote. Unencrypted packet.
			ATUI.array[0] = payload[0];
			ATUI.array[1] = payload[1];
			ATUI.array[2] = payload[2];
			ATUI.array[3] = payload[3];
			return OnPKEResponseReceived(ATUI.uint, &payload[sizeof(uint32_t)]);
		case LinkPackets::LINK_START:
			return OnLinkSwitchOverReceived();
			break;
		case LinkPackets::CHALLENGE:
			ATUI.array[0] = payload[0];
			ATUI.array[1] = payload[1];
			ATUI.array[2] = payload[2];
			ATUI.array[3] = payload[3];
			return OnChallengeRequestReceived(ATUI.uint);
			break;
		case LinkPackets::CLOCK_SYNC:
			OnClockSyncReceived(timestamp, payload);
			break;
		case LinkPackets::REPORT:
			OnInfoReportReceived(payload);
			break;
		default:
			break;
		}

		return true;
	}

protected:
	void OnClockSyncReceived(const uint32_t timestamp, uint8_t* payload)
	{
		switch (LinkState)
		{
		case LinkStateEnum::Disabled:
			break;
		case LinkStateEnum::AwaitingLink:
			break;
		case LinkStateEnum::AwaitingSleeping:
			break;
		case LinkStateEnum::Linking:
			ClockSyncer->OnReceivedLinking(timestamp, payload);
			SetNextRunASAP();
			break;
		case LinkStateEnum::Linked:
			ClockSyncer->OnReceivedLinked(timestamp, payload);
			SetNextRunASAP();
			break;
		default:
			break;
		}
	}

	void OnChallengeReceived(const uint32_t timestamp, uint8_t* payload)
	{
		//TODO:
	}

	void OnInfoReportReceived(uint8_t* payload)
	{
		switch (LinkState)
		{
		case LinkStateEnum::Linking:
		case LinkStateEnum::Linked:
			ReportPending |= payload[0] > 0;

			LinkInfo->UpdatePartnerInfo(payload[1]);
			SetNextRunASAP();
			break;
		default:
			break;
		}
	}

	virtual void OnService()
	{
		switch (LinkState)
		{
		case LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			break;
		case LinkStateEnum::AwaitingLink:
			OnAwaitingLink();
			break;
		case LinkStateEnum::Linking:
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
			{
				UpdateLinkState(LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnLinking();
			}
			break;
		case LinkStateEnum::Linked:
			if (LoLaDriver->IOInfo.GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
			{
				UpdateLinkState(LinkStateEnum::AwaitingLink);
			}
			else
			{
				if (ProcessReportInfo())
				{
					// Sending packet.
				}
				else if (ProcessClockSync())
				{
					// Sending packet.
				}
				else
				{
					OnKeepingLink();
				}
			}
			break;
		case LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

private:
	bool ProcessReportInfo()
	{
		if (ReportPending  // Keep link info up to date.
			|| LinkInfo->GetLastLocalInfoSentElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD) // Check for local report overdue.
		{
			ReportPending = false;
			SendLinkReport(LinkInfo->GetPartnerLastUpdateElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD);
		}
		else if (IOInfo->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION // Check for no received packets in a while.
			|| LinkInfo->GetPartnerLastUpdateElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD) // Check for partner report overdue.
		{
			ReportPending = false;
			SendLinkReport(true);
		}
		else
		{
			return false;
		}

		return true;
	}


protected:
	void ClearSession()
	{
		// Disables encryption and token hop.
		LoLaDriver->CryptoEncoder.Clear();

		LoLaDriver->ChannelManager.StopHopping();
		LoLaDriver->TransmitPowerManager.SetMaxPower();

		KeyExchanger.ClearPartner();
		ClockSyncer->Clear();

		LinkInfo->Reset();

		ReportPending = false;
	}

	bool ProcessClockSync()
	{
		if (ClockSyncer->HasResponse())
		{
			ClockSyncer->GetResponse(GetOutPayload());
			RequestSendPacket(LinkPackets::CLOCK_SYNC);

			return true;
		}

		return false;
	}

	void UpdateLinkState(const LinkStateEnum newState)
	{
		if (LinkState != newState)
		{
			Serial.print(F("UpdateLinkState: "));
			Serial.println(newState);

			ResetStateStartTime();
			ResetLastSentTimeStamp();

			ArrayToUint32 ATUI;

			switch (newState)
			{
			case LinkStateEnum::AwaitingLink:
				ClearSession();
				Enable();
				SetNextRunASAP();
				break;
			case LinkStateEnum::AwaitingSleeping:
				ClearSession();
				SetNextRunDelay(GetSleepDelay());
				break;
			case LinkStateEnum::Linking:
				LinkInfo->SetSessionIdSignature(LoLaDriver->CryptoEncoder.SignContentSharedKey(LinkInfo->GetSessionId()));
				ResetLinkingState();
				LoLaDriver->CryptoEncoder.StartEncryption();
				SetNextRunASAP();
				break;
			case LinkStateEnum::Linked:
				LoLaDriver->CryptoEncoder.SetKeyWithSalt(KeyExchanger.GetSharedKey(), LinkInfo->GetSessionId());
				LoLaDriver->CryptoEncoder.StartTokenHop();
				LoLaDriver->ChannelManager.StartHopping();
				ClockSyncer->StampSynced();
				LinkInfo->StampLinkStarted();
				SetNextRunASAP();
				break;
			case LinkStateEnum::Disabled:
				ClearSession();
				LoLaDriver->Stop();
				Disable();
			default:
				break;
			}

			LinkState = newState;
			OnStateChanged();

			// If link status changed, 
			// notify all link dependent services they can start/stop.
			if (LinkInfo->UpdateLinkStatus(LinkState == LinkStateEnum::Linked))
			{
				LoLaDriver->NotifyServicesLinkStatusChanged();
			}
		}
	}

protected:
	// Info Report packet.
	void SendLinkReport(const bool requestReply)
	{
		GetOutPayload()[0] = requestReply ? UINT8_MAX : 0;
		GetOutPayload()[1] = LoLaDriver->IOInfo.GetRSSINormalized();
		RequestSendPacket(LinkPackets::REPORT);
	}
};
#endif