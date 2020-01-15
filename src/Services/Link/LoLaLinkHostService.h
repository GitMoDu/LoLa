// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum : uint8_t
	{
		NoSession = 0,
		ValidatingPartner = 1,
		WaitingForPKEAck = 2,
		ProcessingPKE = 3
	} AwaitingLinkState = AwaitingLinkEnum::NoSession;




	LinkHostClockSyncer ClockSyncer;

public:
	LoLaLinkHostService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: LoLaLinkService(servicesScheduler, driverScheduler, driver)
		, ClockSyncer()
	{
		driver->SetDuplexSlot(true);
		SetClockSyncer(&ClockSyncer);
	}

	virtual bool Setup()
	{
		if (ClockSyncer.Setup(&SyncedClock))
		{
			return LoLaLinkService::Setup();
		}

		return false;
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA


protected:
	virtual void OnAckFailed(const uint8_t header, const uint8_t id)
	{
		if (header == DefinitionPKEResponse.Header &&
			LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::WaitingForPKEAck)
		{
			AwaitingLinkState = AwaitingLinkEnum::NoSession;
			SetNextRunASAP();
		}
	}

	virtual void OnAckOk(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (header == DefinitionPKEResponse.Header &&
			LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::WaitingForPKEAck &&
			id == SessionId)
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
		switch (AwaitingLinkState)
		{
		case AwaitingLinkEnum::NoSession:
			KeyExchanger.ClearPartner();
			SetNextRunDelay(UINT32_MAX);
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
			SessionSalt = random(UINT32_MAX);

			PreparePKEResponse();
			RequestSendPacket();
			AwaitingLinkState = AwaitingLinkEnum::WaitingForPKEAck;
			break;
		case AwaitingLinkEnum::WaitingForPKEAck:
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKE_CANCEL)
			{
				AwaitingLinkState = AwaitingLinkEnum::NoSession;
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKE_CANCEL - GetElapsedMillisSinceStateStart());
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

	void OnPKERequestReceived(const uint8_t sessionId, const uint32_t protocolCode, uint8_t* remotePublicKey)
	{
		if ((LinkState == LinkStateEnum::AwaitingLink || LinkState == LinkStateEnum::AwaitingSleeping) &&
			protocolCode != ProtocolVersion.GetCode() &&
			SessionId != sessionId && // Force remotes to cycle session id.
			KeyExchanger.SetPartnerPublicKey(remotePublicKey))
		{
			SessionId = sessionId;
			AwaitingLinkState = AwaitingLinkEnum::ValidatingPartner;
			Enable();
			SetNextRunASAP();
		}
	}

	bool OnLinkSwitchOverReceived(const uint8_t sessionId)
	{
		if (LinkState == LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage &&
			SessionId == sessionId)
		{
			LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver;
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
			LinkingState = LinkingStagesEnum::LinkingDone;
			SetNextRunASAP();
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

private:
	void PreparePKEResponse()
	{
		OutPacket.SetDefinition(&DefinitionPKEResponse);
		OutPacket.SetId(SessionId);

		ATUI_S.uint = SessionSalt;
		OutPacket.GetPayload()[0] = ATUI_S.array[0];
		OutPacket.GetPayload()[1] = ATUI_S.array[1];
		OutPacket.GetPayload()[2] = ATUI_S.array[2];
		OutPacket.GetPayload()[3] = ATUI_S.array[3];

		for (uint8_t i = 0; i < KeyExchanger.KEY_CURVE_SIZE; i++)
		{
			OutPacket.GetPayload()[i + sizeof(uint32_t)] = KeyExchanger.GetPublicKeyCompressed()[i];
		}
	}
};
#endif