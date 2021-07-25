// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Link\LoLaLinkService.h>

class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum
	{
		NoSession = 0,
		ValidatingPartner = 1,
		WaitingForPKEAck = 2,
		ProcessingPKE = 3
	} AwaitingLinkState = AwaitingLinkEnum::NoSession;

	LinkHostClockSyncer ClockSyncer;

public:
	LoLaLinkHostService(Scheduler* scheduler, LoLaPacketDriver* driver)
		: LoLaLinkService(scheduler, driver, &ClockSyncer)
		, ClockSyncer(&driver->SyncedClock)
	{
		driver->SetDuplexSlot(true);
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA

protected:
	virtual void OnAckFailed(const uint8_t header)
	{
		if (LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::WaitingForPKEAck)
		{
			SetNextRunASAP();
		}
	}

	virtual void OnAckOk(const uint8_t header)
	{
		if (LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::WaitingForPKEAck)
		{
			AwaitingLinkState = AwaitingLinkEnum::ProcessingPKE;
			ProcessingDHState = ProcessingDHEnum::GeneratingSecretKey;
			SetNextRunASAP();
		}
	}

	virtual uint32_t GetSleepDelay()
	{
		return LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD;
	}

	virtual void ResetLinkingState()
	{
		AwaitingLinkState = AwaitingLinkEnum::NoSession;
	}

	void OnAwaitingLink()
	{
		ArrayToUint32 ATUI;

		switch (AwaitingLinkState)
		{
		case AwaitingLinkEnum::NoSession:
			KeyExchanger.ClearPartner();
			LoLaDriver->ChannelManager.NextRandomChannel();
			UpdateLinkState(LinkStateEnum::AwaitingSleeping);
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

			//TODO: Replace with crypt RNG.
			LinkInfo->SetSessionId(random(INT32_MAX) * 2);

			SendPKEResponse();
			AwaitingLinkState = AwaitingLinkEnum::WaitingForPKEAck;
			break;
		case AwaitingLinkEnum::WaitingForPKEAck:
			if (GetElapsedSinceLastSent() > ILOLA_DUPLEX_PERIOD_MILLIS / 2)
			{
				SendPKEResponse();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
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
				//TODO: Replace with crypt RNG.
				LoLaDriver->SyncedClock.SetOffsetSeconds(random(INT32_MAX) * 2);

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

	virtual void OnPKERequestReceived(const uint32_t protocolCode, uint8_t* remotePublicKey)
	{
		if ((LinkState == LinkStateEnum::AwaitingLink || LinkState == LinkStateEnum::AwaitingSleeping) &&
			protocolCode == LinkInfo->LinkProtocolVersion &&
			KeyExchanger.SetPartnerPublicKey(remotePublicKey))
		{
			AwaitingLinkState = AwaitingLinkEnum::ValidatingPartner;
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			if (!IsSending()) {
				SetNextRunASAP();
			}
		}
	}

	virtual bool OnChallengeRequestReceived(const uint32_t signedSessionId)
	{
		if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ChallengeStage &&
			signedSessionId == LinkInfo->GetSessionIdSignature())
		{
			LinkingState = LinkingStagesEnum::ClockSyncStage;
			ClearSendRequest();

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual bool OnLinkSwitchOverReceived()
	{
		if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage)
		{
			LinkingState = LinkingStagesEnum::LinkProtocolSwitchOver;
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
			// Waiting for remote send the challenge request with signed session id.
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		case LinkingStagesEnum::ClockSyncStage:
			if (ProcessClockSync())
			{
				// Sending packet.
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case LinkingStagesEnum::LinkProtocolSwitchOver:
			// All linking stages complete, we have a link.
			UpdateLinkState(LinkStateEnum::Linked);
			break;
		default:
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			break;
		}
	}

private:
	void SendPKEResponse()
	{
		ArrayToUint32 ATUI;

		ATUI.uint = LinkInfo->GetSessionId();
		GetOutPayload()[0] = ATUI.array[0];
		GetOutPayload()[1] = ATUI.array[1];
		GetOutPayload()[2] = ATUI.array[2];
		GetOutPayload()[3] = ATUI.array[3];

		uint8_t* LocalPublicKeyCompressed = KeyExchanger.GetPublicKeyCompressed();

		for (uint8_t i = 0; i < KeyExchanger.KEY_CURVE_SIZE; i++)
		{
			GetOutPayload()[sizeof(uint32_t) + i] = LocalPublicKeyCompressed[i];
		}

		RequestSendPacket(LinkPackets::PKE_RESPONSE);
	}
};
#endif