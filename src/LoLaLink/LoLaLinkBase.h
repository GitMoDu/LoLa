// LoLaLinkBase.h

#ifndef _LOLA_LINK_BASE_h
#define _LOLA_LINK_BASE_h

#include <LoLaLink\LoLaLinkService.h>
#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>

enum LinkStateEnum
{
	Disabled,
	AwaitingLink,
	AwaitingSleeping,
	Linking,
	Linked
};

template<const uint8_t MaxPacketSize, const uint8_t MaxLinkListeners = 4, const uint8_t MaxPacketListeners = 8>
class LoLaLinkBase : protected LoLaLinkService<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>
{
protected:
	// Host/Remote clock syncer.
	//LoLaLinkClockSyncer ClockSyncer;

	// Crypto key exchanger.
	LoLaCryptoKeyExchanger KeyExchanger;

	// Runtime helpers.
	uint32_t StateStartTime = 0;

	// State.
	LinkStateEnum LinkState = LinkStateEnum::Disabled;

	// Link report tracking.
	bool ReportPending = false;

	//// Shared Sub state helpers.
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

	// Used as a binary slot for now, can be refactored for multiple slots.
	const uint8_t DuplexSlot = 0;

	using LoLaLinkService<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::NotifyLinkStateUpdate;

public:
	LoLaLinkBase(Scheduler* scheduler, ILoLaPacketDriver* driver, const uint8_t duplexSlot)
		: LoLaLinkService<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>(scheduler, driver)
		, DuplexSlot(duplexSlot)
		, KeyExchanger()
	{
	}

	virtual const bool Setup()
	{
		if (LoLaLinkService<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::Setup())
		{
			//KeyExchanger.Setup();
			//TODO: Replace with constant random address, bitcoin style.
			// the public key is the address.
			//TODO: Sign the address with the certificate.
			//KeyExchanger.GenerateNewKeyPair();

			return true;
		}

		return false;
	}

	/*void Start()
	{
		Driver->DriverStart();
		UpdateLinkState(LinkStateEnum::AwaitingLink);
	}

	void Stop()
	{
		UpdateLinkState(LinkStateEnum::Disabled);
	}*/

protected:
	// Override just to guard against send slot.
	virtual const bool SendPacket(uint8_t* payload, const uint8_t payloadSize, const uint8_t header, const bool hasAck)
	{
		if (IsInSendSlot())
		{
			return LoLaLinkService<MaxPacketSize, MaxLinkListeners, MaxPacketListeners>::SendPacket(payload, payloadSize, header, hasAck);
		}
		else
		{
			return false;
		}
	}

protected:
	void UpdateLinkState(const LinkStateEnum newState)
	{
		if (LinkState != newState)
		{
			StateStartTime = millis();

			// Previous state.
			switch (LinkState)
			{
			case LinkStateEnum::Disabled:
				break;
			case LinkStateEnum::AwaitingLink:
				break;
			case LinkStateEnum::AwaitingSleeping:
				break;
			case LinkStateEnum::Linking:
				break;
			case LinkStateEnum::Linked:
				NotifyLinkStateUpdate(false);
				break;
			default:
				break;
			}

			LinkState = newState;

			if (LinkState == LinkStateEnum::Linked)
			{
				NotifyLinkStateUpdate(true);
			}
		}

	}

	void ResetStateStartTime()
	{
		StateStartTime = millis();
	}

	const uint32_t GetElapsedMillisSinceStateStart()
	{
		if (StateStartTime == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return millis() - StateStartTime;
		}
	}

private:
	const bool IsInSendSlot()
	{
		////ClockSyncer
		////DuplexSlot
		return true;
	}
};
#endif