// AbstractLoLaLinkPacket.h

#ifndef _ABSTRACT_LOLA_LINK_PACKET_
#define _ABSTRACT_LOLA_LINK_PACKET_


#include "AbstractLoLaReceiver.h"

#include <ILoLaPacketDriver.h>

#include "..\..\Link\IChannelHop.h"

#include "..\..\Link\IDuplex.h"

#include "..\..\Link\LoLaLinkDefinition.h"
#include "..\..\Link\LoLaLinkSession.h"





/// <summary>
/// Implements PacketServce functionality and leaves interfaces open for implementation.
/// Implements channel management.
/// As a partial abstract class, it implements the following ILoLaLink calls:
///		- CanSendPacket.
///		- GetRxChannel.
/// </summary>
template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaLinkPacket
	: public AbstractLoLaReceiver<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
	, public virtual IChannelHop::IHopListener
{
private:
	using BaseClass = AbstractLoLaReceiver<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::LinkStage;
	using BaseClass::SyncClock;
	using BaseClass::PacketService;
	using BaseClass::SendCounter;
	using BaseClass::ZeroTimestamp;

	using BaseClass::RawInPacket;
	using BaseClass::RawOutPacket;

	using BaseClass::RegisterPacketReceiverInternal;
	using BaseClass::PostLinkState;
	using BaseClass::GetSendDuration;

protected:
	/// <summary>
	/// HKDF Expanded key, with extra seeds.
	/// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey;

	LoLaRandom RandomSource; // Cryptographic Secure(ish) Random Number Generator.

protected:
	// Collision avoidance time slotting.
	IDuplex* Duplex;

private:
	/// <summary>
	/// Channel Hopper calls back when it is time to update the RX channel, if it's not up to date.
	/// </summary>
	IChannelHop* ChannelHopper;

	Timestamp HopTimestamp;

	uint32_t StageStartTime = 0;

	const bool IsLinkHopper;

public:
	AbstractLoLaLinkPacket(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex, 
		IChannelHop* hop)
		: BaseClass(scheduler, encoder, driver, clockSource, timerSource)
		, IChannelHop::IHopListener()
		, Duplex(duplex)
		, ChannelHopper(hop)
		, IsLinkHopper(hop->IsHopper())
		, RandomSource(entropySource)
		, HopTimestamp()
	{}

#if defined(DEBUG_LOLA)
	virtual void DebugClock() final
	{
		//SyncClock.Debug();
	}
#endif

	virtual const bool Setup()
	{
		if (RandomSource.Setup() && 
			ChannelHopper != nullptr &&
			ChannelHopper->Setup(this, &SyncClock, GetSendDuration(0)))
		{
			return BaseClass::Setup();
		}
		else
		{
			return false;
		}
	}

	/// <summary>
	/// IChannelHopper overrides.
	/// </summary>
public:
	virtual void OnChannelHopTime() final
	{
		PacketService.RefreshChannel();
	}

	virtual const bool HasLink() final
	{
		return LinkStage == LinkStageEnum::Linked;
	}

	/// <summary>
	/// ILoLaLink overrides.
	/// </summary>
public:
	virtual const bool RegisterPacketReceiver(ILinkPacketReceiver* listener, const uint8_t port) final
	{
		if (port < LoLaLinkDefinition::MAX_DEFINITION_PORT)
		{
			return RegisterPacketReceiverInternal(listener, port);
		}
		return false;
	}

	virtual const bool CanSendPacket(const uint8_t payloadSize) final
	{
		if (LinkStage == LinkStageEnum::Linked && PacketService.CanSendPacket())
		{
			SyncClock.GetTimestamp(HopTimestamp);
			HopTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

			return Duplex->IsInSlot(HopTimestamp.GetRollingMicros());
		}
		else
		{
			return false;
		}
	}

	/// <summary>
	/// IPacketServiceListener overrides.
	/// </summary>
public:
	virtual const uint8_t GetRxChannel() final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linking:
			return ChannelHopper->GetFixedChannel();
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return Session.GetPrngHopChannel(ChannelHopper->GetTimedHopIndex());
			}
			else
			{
				return ChannelHopper->GetFixedChannel();
			}
		default:
			break;
		}

		return 0;
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		if (LinkStage == LinkStageEnum::Linked &&
			linkStage != LinkStageEnum::Linked)
		{
			ChannelHopper->OnLinkStopped();
		}

		if (linkStage != LinkStage)
		{
			switch (LinkStage)
			{
			case LinkStageEnum::Linked:
				PostLinkState(false);
				break;
			default:
				break;
			}

			LinkStage = linkStage;
			StageStartTime = millis();

			switch (linkStage)
			{
			case LinkStageEnum::Disabled:
				Task::disable();
				break;
			case LinkStageEnum::AwaitingLink:
				SendCounter = RandomSource.GetRandomShort();
				PacketService.RefreshChannel();
				break;
			case LinkStageEnum::Linking:
				break;
			case LinkStageEnum::Linked:
				ChannelHopper->OnLinkStarted();
				PostLinkState(true);
				Task::enable();
				break;
			default:
				Task::enable();
				break;
			}
		}
	}

protected:
	///// <summary>
	///// During Link, if the configured channel management is a hopper,
	///// every N us the channel switches.
	///// </summary>
	///// <param name="rollingMicros">[0;UINT32_MAX]</param>
	///// <returns>Normalized Tx Chanel.</returns>
	virtual const uint8_t GetTxChannel(const uint32_t rollingMicros) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linking:
			return ChannelHopper->GetBroadcastChannel();
		case LinkStageEnum::Linked:
			if (IsLinkHopper)
			{
				return Session.GetPrngHopChannel(ChannelHopper->GetHopIndex(rollingMicros));
			}
			else
			{
				return ChannelHopper->GetFixedChannel();
			}
		default:
			break;
		}

		return 0;
	}


protected:
	const uint32_t GetStageElapsedMillis()
	{
		return millis() - StageStartTime;
	}

	void ResetStageStartTime()
	{
		StageStartTime = millis();
	}

	void SetHopperFixedChannel(const uint8_t channel)
	{
		ChannelHopper->SetFixedChannel(channel);
		PacketService.RefreshChannel();
	}
};
#endif