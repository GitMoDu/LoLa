// AbstractLoLaLinkPacket.h

#ifndef _ABSTRACT_LOLA_LINK_PACKET_
#define _ABSTRACT_LOLA_LINK_PACKET_


#include "AbstractLoLa.h"

#include <ILoLaPacketDriver.h>

#include "..\..\Link\IChannelHop.h"
#include "..\..\Link\PacketEventEnum.h"
#include "..\..\Link\IDuplex.h"
#include "..\..\Link\LoLaPacketService.h"
#include "..\..\Link\LoLaLinkDefinition.h"
#include "..\..\Link\LoLaLinkSession.h"
#include "..\..\Link\LinkStageEnum.h"

#include "..\..\Clock\SynchronizedClock.h"
#include "..\..\Crypto\LoLaCryptoEncoderSession.h"



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
	: public AbstractLoLa<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
	, public virtual IPacketServiceListener
	, public virtual IChannelHop::IHopListener
{
private:
	using BaseClass = AbstractLoLa<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::RawInPacket;
	using BaseClass::RawOutPacket;

	using BaseClass::RegisterPacketReceiverInternal;
	using BaseClass::PostLinkState;

protected:
	Timestamp ZeroTimestamp;

protected:
	// Packet service instance, templated to max packet size and reference low latency timeouts.
	LoLaPacketService<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> PacketService;

	// Duplex, Channel Hop and Cryptography depend on a synchronized clock between host and remote.
	SynchronizedClock SyncClock;

	// Packet Driver for the PHY layer.
	ILoLaPacketDriver* Driver;

	// Collision avoidance time slotting.
	IDuplex* Duplex;

private:
	/// <summary>
	/// Channel Hopper calls back when it is time to update the RX channel, if it's not up to date.
	/// </summary>
	IChannelHop* ChannelHopper;

	const bool IsLinkHopper;

	Timestamp HopTimestamp{};

protected:
	// Current Link Stage for packet handling.
	volatile LinkStageEnum LinkStage = LinkStageEnum::Disabled;

	LoLaCryptoEncoderSession Session;

protected:
	virtual void OnEvent(const PacketEventEnum packetEvent) {}

private:
	// Runtime helpers.
	uint32_t SendShortDurationMicros = 0;
	uint32_t SendVariableDurationMicros = 0;

	uint32_t StageStartTime = 0;

public:
	AbstractLoLaLinkPacket(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler)
		, IPacketServiceListener()
		, IChannelHop::IHopListener()
		, PacketService(scheduler, this, driver, RawInPacket, RawOutPacket)
		, Driver(driver)
		, Duplex(duplex)
		, SyncClock(clockSource, timerSource)
		, ChannelHopper(hop)
		, IsLinkHopper(hop->IsHopper())
		, Session(entropySource)
		, ZeroTimestamp()
	{}

#if defined(DEBUG_LOLA)
	virtual void DebugClock()
	{
		//SyncClock.Debug();
	}
#endif

	virtual const bool Setup()
	{
		if (PacketService.Setup() &&
			Session.Setup() &&
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
		if (port <= LoLaLinkDefinition::MAX_DEFINITION_PORT)
		{
			return RegisterPacketReceiverInternal(listener, port);
		}
		return false;
	}

	virtual const bool CanSendPacket(const uint8_t payloadSize) final
	{
		if (LinkStage == LinkStageEnum::Linked &&
			PacketService.CanSendPacket())
		{
			SyncClock.GetTimestamp(HopTimestamp);
			HopTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

			return Duplex->IsInSlot(HopTimestamp.GetRollingMicros());
		}
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		if (LinkStage == LinkStageEnum::Linked &&
			linkStage != LinkStageEnum::Linked)
		{
			ChannelHopper->OnLinkStopped();
		}

		///
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
				PacketService.RefreshChannel();
				break;
				//case LinkStageEnum::TransitionToLinking:
				//	break;	
			case LinkStageEnum::Linking:
				break;
				//case LinkStageEnum::TransitionToLinked:
				//	break;
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
	const bool SetSendCalibration(const uint32_t shortDuration, const uint32_t longDuration)
	{
		if (shortDuration > 0 && longDuration > shortDuration)
		{
			SendShortDurationMicros = shortDuration;
			SendVariableDurationMicros = longDuration - shortDuration;
			return true;
		}

		return false;
	}

	///// <summary>
	///// During Link, if the configured channel management is a hopper,
	///// every N us the channel switches.
	///// </summary>
	///// <param name="rollingMicros">[0;UINT32_MAX]</param>
	///// <returns>Normalized Tx Chanel.</returns>
	const uint8_t GetTxChannel(const uint32_t rollingMicros)
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
			//case LinkStageEnum::TransitionToLinking:
		case LinkStageEnum::Linking:
			return ChannelHopper->GetBroadcastChannel();
			//case LinkStageEnum::TransitionToLinked:
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

	virtual const uint8_t GetRxChannel() final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			return ChannelHopper->GetBroadcastChannel();
			//case LinkStageEnum::TransitionToLinking:
		case LinkStageEnum::Linking:
			return ChannelHopper->GetFixedChannel();
			//case LinkStageEnum::TransitionToLinked:
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
	//virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	//{
	//	//if (linkStage != LinkStage)
	//	//{
	//	//	switch (LinkStage)
	//	//	{
	//	//	case LinkStageEnum::Linked:
	//	//		PostLinkState(false);
	//	//		break;
	//	//	default:
	//	//		break;
	//	//	}

	//	//	LinkStage = linkStage;
	//	//	StageStartTime = millis();

	//	//	switch (linkStage)
	//	//	{
	//	//	case LinkStageEnum::Disabled:
	//	//		Task::disable();
	//	//		break;
	//	//	case LinkStageEnum::AwaitingLink:
	//	//		break;
	//	//		//case LinkStageEnum::TransitionToLinking:
	//	//		//	break;	
	//	//	case LinkStageEnum::Linking:
	//	//		break;
	//	//		//case LinkStageEnum::TransitionToLinked:
	//	//		//	break;
	//	//	case LinkStageEnum::Linked:
	//	//		PostLinkState(true);
	//	//		Task::enable();
	//	//		break;
	//	//	default:
	//	//		Task::enable();
	//	//		break;
	//	//	}
	//	//}
	//}

	const uint32_t GetStageElapsedMillis()
	{
		return millis() - StageStartTime;
	}

	void ResetStageStartTime()
	{
		StageStartTime = millis();
	}

	const uint32_t GetSendDuration(const uint8_t payloadSize)
	{
		return SendShortDurationMicros
			+ ((SendVariableDurationMicros * payloadSize) / LoLaPacketDefinition::MAX_PAYLOAD_SIZE)
			+ Driver->GetTransmitDurationMicros(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

	const uint32_t GetReplyDuration(const uint8_t requestPayloadSize, const uint8_t replyPayloadSize)
	{
		return LoLaLinkDefinition::REPLY_BASE_TIMEOUT_MICROS + GetSendDuration(requestPayloadSize) + GetSendDuration(replyPayloadSize);
	}

	void SetHopperFixedChannel(const uint8_t channel)
	{
		ChannelHopper->SetFixedChannel(channel);
		PacketService.RefreshChannel();
	}
};
#endif