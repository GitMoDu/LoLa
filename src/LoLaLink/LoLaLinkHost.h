// LoLaLinkHost.h

#ifndef _LOLALINKHOST_h
#define _LOLALINKHOST_h

#include <LoLaLink\LoLaLinkBase.h>

template<const uint8_t MaxPacketSize, const uint8_t MaxLinkListeners = 4, const uint8_t MaxPacketListeners = 8>
class LoLaLinkHost : protected LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>
{
private:
	enum AwaitingLinkEnum
	{
		NoSession,
		ValidatingPartner,
		WaitingForPKEAck,
		NackPKE,
		ProcessingPKE
	} AwaitingLinkState = AwaitingLinkEnum::NoSession;

	using LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::LinkState;
	using LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::ReceiveTimeTimestamp;
	using LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::KeyExchanger;

public:
	LoLaLinkHost(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>(scheduler, driver, 0)
	{
	}

	virtual const bool Setup()
	{
		if (LoLaLinkBase<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::Setup())
		{
			return true;
		}

		return false;
	}

protected:
	// PacketLink overrides
	virtual void OnService()
	{
		switch (LinkState)
		{
		case LinkStateEnum::AwaitingLink:
			OnAwaitingLink();
			break;
		case LinkStateEnum::AwaitingSleeping:
			Task::disable();
			break;
		case LinkStateEnum::Linking:
			Task::enable();
			break;
		case LinkStateEnum::Linked:
			Task::enable();
			break;
		case LinkStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	virtual void OnLinkReceiveAck()
	{
		uint32_t receivedTimestamp = ReceiveTimeTimestamp;
		switch (LinkState)
		{
		case LinkStateEnum::AwaitingLink:
			Task::enable();
			break;
		case LinkStateEnum::AwaitingSleeping:
			Task::enable();
			break;
		case LinkStateEnum::Linking:
			Task::enable();
			break;
		case LinkStateEnum::Linked:
			Task::enable();
			break;
		case LinkStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	virtual void OnLinkReceiveNack()
	{
		if (LinkState == LinkStateEnum::AwaitingLink &&
			AwaitingLinkState == AwaitingLinkEnum::WaitingForPKEAck)
		{
			AwaitingLinkState == AwaitingLinkEnum::NackPKE;
			Task::enable();
		}
	}

	virtual void OnLinkReceive(const uint8_t contentSize)
	{
		// Filter packet acceptance by state.
		switch (LinkState)
		{
		case LinkStateEnum::AwaitingLink:
			Task::enable();
			break;
		case LinkStateEnum::AwaitingSleeping:
			Task::enable();
			break;
		case LinkStateEnum::Linking:
			Task::enable();
			break;
		case LinkStateEnum::Linked:
			Task::enable();
			break;
		case LinkStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

private:
	void OnAwaitingLink()
	{
		//ArrayToUint32 ATUI;

		switch (AwaitingLinkState)
		{
		case AwaitingLinkEnum::NoSession:
			KeyExchanger.ClearPartner();
			//ChannelManager.NextRandomChannel();
			UpdateLinkState(LinkStateEnum::AwaitingSleeping);
			break;
		//case AwaitingLinkEnum::ValidatingPartner:
		//	if (!KeyExchanger.GotPartnerPublicKey())
		//	{
		//		AwaitingLinkState = AwaitingLinkEnum::NoSession;
		//		SetNextRunASAP();
		//		return;
		//	}

		//	//TODO: Filter address from known list.
		//	if (!true)
		//	{
		//		AwaitingLinkState = AwaitingLinkEnum::NoSession;
		//		SetNextRunASAP();
		//		return;
		//	}

		//	//TODO: Replace with crypt RNG.
		//	LinkInfo->SetSessionId(random(INT32_MAX) * 2);

		//	SendPKEResponse();
		//	AwaitingLinkState = AwaitingLinkEnum::WaitingForPKEAck;
		//	break;
		//case AwaitingLinkEnum::WaitingForPKEAck:
		//	if (GetElapsedSinceLastSent() > ILOLA_DUPLEX_PERIOD_MILLIS / 2)
		//	{
		//		SendPKEResponse();
		//	}
		//	else
		//	{
		//		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		//	}
		//	break;
		//case AwaitingLinkEnum::ProcessingPKE:
		//	switch (ProcessingDHState)
		//	{
		//	case ProcessingDHEnum::GeneratingSecretKey:
		//		KeyExchanger.GenerateSharedKey();

		//		ProcessingDHState = ProcessingDHEnum::ExpandingKey;
		//		SetNextRunASAP();
		//		break;
		//	case ProcessingDHEnum::ExpandingKey:
		//		//TODO: Replace with crypt RNG.
		//		LoLaDriver->SyncedClock.SetOffsetSeconds(random(INT32_MAX) * 2);

		//		// All set to start linking.
		//		UpdateLinkState(LinkStateEnum::Linking);
		//		break;
		//	default:
		//		AwaitingLinkState = AwaitingLinkEnum::NoSession;
		//		SetNextRunASAP();
		//		break;
		//	}
		//	break;
		default:
			AwaitingLinkState = AwaitingLinkEnum::NoSession;
			Task::enable();
			break;
		}
	}
};
#endif