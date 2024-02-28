// AbstractDiscoveryService.h

#ifndef _ABSTRACT_DISCOVERY_SERVICE_
#define _ABSTRACT_DISCOVERY_SERVICE_

#include "..\..\LoLaServices\TemplateLoLaService.h"
#include "DiscoveryDefinitions.h"

using namespace DiscoveryDefinitions;

/// <summary>
/// Discovers and acknowledges the existance of a partner service,
/// on the same port and using the same service id.
/// Linked callbacks only after discovery is complete.
/// Can be piggybacked on by using TemplateHeaderDefinitions. 
/// Header UINT8_MAX is reserved for Discovery Service.
/// Rejects packets if not discovered, and requests partner to stops service.
/// </summary>
/// <typeparam name="Port">The port registered for this service.</typeparam>
/// <typeparam name="ServiceId">Unique service identifier.</typeparam>
/// <typeparam name="MaxSendPayloadSize">The max packet payload sent by this service.
/// Effective value will always be at least as big as DiscoveryDefinition::PAYLOAD_SIZE.</typeparam>
template<const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize = 0>
class AbstractDiscoveryService
	: public TemplateLoLaService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>
	, public virtual ILinkListener
{
private:
	using BaseClass = TemplateLoLaService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>;

	static constexpr int8_t DISCOVERY_SLOT_COUNT = 20;
	static constexpr uint16_t DISCOVERY_SLOT_TOLERANCE_MICROS = 3000;
	static constexpr uint32_t DISCOVERY_SLOT_PERIOD_MICROS = ((uint32_t)LoLaLinkDefinition::DUPLEX_PERIOD_MAX_MICROS * 3) + DISCOVERY_SLOT_TOLERANCE_MICROS;
	static constexpr uint32_t DISCOVERY_INCREMENTAL_PERIOD_MICROS = (DISCOVERY_SLOT_PERIOD_MICROS / 2) - 1;

	static constexpr uint32_t DISCOVERY_TIMEOUT_MICROS = DISCOVERY_SLOT_PERIOD_MICROS * DISCOVERY_SLOT_COUNT;

	enum class DiscoveryStateEnum : uint8_t
	{
		WaitingForLink,
		Discovering,
		StartingSlot,
		MatchingSlot,
		MatchedSlot,
		WaitingForSlotEnd,
		Running
	};

protected:
	using BaseClass::LoLaLink;
	using BaseClass::RequestSendCancel;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::RegisterPacketListener;
	using BaseClass::PacketThrottle;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::OutPacket;

protected:
	/// <summary>
	/// Fires when the the service has been discovered.
	/// </summary>
	virtual void OnServiceStarted() { Task::disable(); }

	/// <summary>
	/// Fires when the the service has been lost.
	/// </summary>
	virtual void OnServiceEnded() { Task::disable(); }

	/// <summary>
	/// Fires when the the service partner can't be found.
	/// </summary>
	virtual void OnDiscoveryFailed() { Task::disable(); }

	/// <summary>
	/// The service can ride this task's callback while linked,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnLinkedServiceRun() { Task::disable(); }

	/// <summary>
	/// Fires when the service packet send request
	///  failed after transmission request.
	/// </summary>
	virtual void OnLinkedSendRequestFail() { }

	/// <summary>
	/// After discovery is completed, any non-discovery packets will be routed to the service.
	/// </summary>
	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}

private:
	DiscoveryStateEnum DiscoveryState = DiscoveryStateEnum::WaitingForLink;
	uint8_t LocalSlot = 0;
	bool ReplyPending = false;

public:
	AbstractDiscoveryService(Scheduler& scheduler, ILoLaLink* loLaLink)
		: BaseClass(scheduler, loLaLink)
	{}

public:
	virtual const bool Setup()
	{
		return BaseClass::Setup()
			&& LoLaLink->RegisterLinkListener(this)
			&& RegisterPacketListener(Port);
	}

	/// <summary>
	/// Link state update callback.
	/// Overrides listener and dispatches event to Service callbacks.
	/// Takes this opportunity to clear any pending sends.
	/// </summary>
	/// <param name="hasLink">True has been acquired, false if lost. Same value as ILoLaLink::HasLink()</param>
	virtual void OnLinkStateUpdated(const bool hasLink) final
	{
		if (hasLink)
		{
			if (DiscoveryState == DiscoveryStateEnum::WaitingForLink)
			{
				Task::enableDelayed(1);
				DiscoveryState = DiscoveryStateEnum::Discovering;
			}
			else
			{
				Task::enableIfNot();
			}
		}
		else
		{
			RequestSendCancel();
			Task::disable();
			switch (DiscoveryState)
			{
			case DiscoveryStateEnum::Running:
				OnServiceEnded();
				break;
			default:
				break;
			}
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
		}
	}

	/// <summary>
	/// Captures link packet event and forwards to service, if not own header.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="port">Which registered port was the packet sent to.</param>
	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port == Port
			&& payload[HeaderDefinition::HEADER_INDEX] == DiscoveryDefinition::HEADER)
		{
			if (payloadSize == DiscoveryDefinition::PAYLOAD_SIZE
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX] == (uint8_t)(ServiceId)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 1] == (uint8_t)(ServiceId >> 8)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 2] == (uint8_t)(ServiceId >> 16)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 3] == (uint8_t)(ServiceId >> 24))
			{
				const bool remoteReject = DiscoveryDefinition::GetRejectFromMeta(payload[DiscoveryDefinition::PAYLOAD_META_INDEX]);

				if (remoteReject)
				{
					if (DiscoveryState == DiscoveryStateEnum::Running)
					{
						RequestSendCancel();
						Task::disable();
						OnServiceEnded();
					}
					DiscoveryState = DiscoveryStateEnum::WaitingForLink;
					Serial.println(F("Rx Rejection."));
				}
				else if (DiscoveryState != DiscoveryStateEnum::WaitingForLink)
				{
					const uint8_t remoteSlot = DiscoveryDefinition::GetSlotFromMeta(payload[DiscoveryDefinition::PAYLOAD_META_INDEX]);
					const bool remoteAck = DiscoveryDefinition::GetAckFromMeta(payload[DiscoveryDefinition::PAYLOAD_META_INDEX]);
					const bool slotsMatch = remoteSlot == LocalSlot;

					switch (DiscoveryState)
					{
					case DiscoveryStateEnum::Discovering:
						DiscoveryState = DiscoveryStateEnum::StartingSlot;
						LocalSlot = remoteSlot;
						ResetPacketThrottle();
						Task::delay(0);
						break;
					case DiscoveryStateEnum::MatchingSlot:
						if ((LoLaLink->GetLinkElapsed() + DISCOVERY_INCREMENTAL_PERIOD_MICROS) < GetTurnoverTimestamp(LocalSlot)
							&& slotsMatch)
						{
							DiscoveryState = DiscoveryStateEnum::MatchedSlot;
						}
						LocalSlot = GetNextSlot(LocalSlot, remoteSlot);
						ResetPacketThrottle();
						break;
					case DiscoveryStateEnum::MatchedSlot:
						if ((LoLaLink->GetLinkElapsed() + DISCOVERY_SLOT_TOLERANCE_MICROS) < GetTurnoverTimestamp(LocalSlot)
							&& slotsMatch
							&& remoteAck)
						{
							ReplyPending = true;
							DiscoveryState = DiscoveryStateEnum::WaitingForSlotEnd;
						}
						LocalSlot = GetNextSlot(LocalSlot, remoteSlot);
						ResetPacketThrottle();
						break;
					case DiscoveryStateEnum::WaitingForSlotEnd:
						LocalSlot = GetNextSlot(LocalSlot, remoteSlot);
						if (slotsMatch)
						{
							ReplyPending = false;
						}
						else if (!remoteAck)
						{
							ResetPacketThrottle();
							DiscoveryState = DiscoveryStateEnum::MatchingSlot;
							Serial.println(F("Retry on Rx WaitingForSlotEnd 1."));
						}
						else
						{
							ResetPacketThrottle();
							DiscoveryState = DiscoveryStateEnum::MatchedSlot;
							Serial.println(F("Retry on Rx WaitingForSlotEnd 2."));
						}
						break;
					case DiscoveryStateEnum::Running:
					default:
						Serial.print(F("Unexpected Discovery packet. State = "));
						Serial.println((uint8_t)DiscoveryState);
						break;
					}
				}
			}
		}
		else if (DiscoveryState == DiscoveryStateEnum::Running)
		{
			OnLinkedPacketReceived(timestamp, payload, payloadSize, port);
		}
		else if (DiscoveryState == DiscoveryStateEnum::WaitingForLink)
		{
			ReplyPending = true;
			Task::enableDelayed(0);
			Serial.println(F("Rx Rejected."));
		}
		else
		{
			ReplyPending = true;
			Task::enableDelayed(0);
			Serial.println(F("Discovery Rejected Packet."));
		}
	}

protected:
	virtual void OnServiceRun() final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::WaitingForLink:
			if (ReplyPending)
			{
				if (RequestSendDiscovery(true, false))
				{
					ReplyPending = false;
				}
			}
			else
			{
				Task::disable();
			}
			break;
		case DiscoveryStateEnum::Discovering:
			if (!DiscoveryTimedOut())
			{
				if ((LoLaLink->GetLinkElapsed() + DISCOVERY_INCREMENTAL_PERIOD_MICROS) > GetTurnoverTimestamp(LocalSlot))
				{
					LocalSlot++;
					Task::delay(0);
				}
				else if (PacketThrottle())
				{
					RequestSendDiscovery(false, false);
				}
				else { Task::delay(0); }
			}
			break;
		case DiscoveryStateEnum::StartingSlot:
			if (LoLaLink->GetLinkElapsed() < DISCOVERY_TIMEOUT_MICROS
				&& StartSlot())
			{
				Task::delay(0);
				DiscoveryState = DiscoveryStateEnum::MatchingSlot;
				if ((LoLaLink->GetLinkElapsed() + DISCOVERY_INCREMENTAL_PERIOD_MICROS) >= GetTurnoverTimestamp(LocalSlot))
				{
					LocalSlot++;
					Task::delay(0);
				}
			}
			else
			{
				DiscoveryState = DiscoveryStateEnum::WaitingForLink;
				RequestSendCancel();
				OnDiscoveryFailed();
			}
			break;
		case DiscoveryStateEnum::MatchingSlot:
			if (!DiscoveryTimedOut())
			{
				if (LocalSlot >= DISCOVERY_SLOT_COUNT)
				{
					DiscoveryState = DiscoveryStateEnum::WaitingForLink;
					RequestSendCancel();
					OnDiscoveryFailed();
				}
				else
				{
					Task::delay(0);
					if ((LoLaLink->GetLinkElapsed() + DISCOVERY_INCREMENTAL_PERIOD_MICROS) > GetTurnoverTimestamp(LocalSlot))
					{
						LocalSlot++;
					}
					if (PacketThrottle())
					{
						RequestSendDiscovery(false, false);
					}
				}
			}
			break;
		case DiscoveryStateEnum::MatchedSlot:
			if (!DiscoveryTimedOut())
			{
				if (LocalSlot >= DISCOVERY_SLOT_COUNT)
				{
					DiscoveryState = DiscoveryStateEnum::WaitingForLink;
					ReplyPending = true;
					RequestSendCancel();
					OnDiscoveryFailed();
				}
				else if ((LoLaLink->GetLinkElapsed()) > GetTurnoverTimestamp(LocalSlot))
				{
					LocalSlot++;
					Task::delay(0);
					ResetPacketThrottle();
				}

				if (PacketThrottle())
				{
					RequestSendDiscovery(false, true);
				}
			}
			break;
		case DiscoveryStateEnum::WaitingForSlotEnd:
			Task::delay(0);
			if (ReplyPending)
			{
				if (!DiscoveryTimedOut())
				{
					if (PacketThrottle())
					{
						RequestSendDiscovery(false, true);
					}
				}
			}
			else if (LoLaLink->GetLinkElapsed() >= GetTurnoverTimestamp(LocalSlot))
			{
				DiscoveryState = DiscoveryStateEnum::Running;
				Serial.print(F("Slot finished with ("));
				Serial.print(LocalSlot);
				Serial.println(')');

				OnServiceStarted();
			}
			break;
		case DiscoveryStateEnum::Running:
			OnLinkedServiceRun();
			break;
		default:
			break;
		}
	}

	virtual void OnSendRequestTimeout() final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::Running:
			OnLinkedSendRequestFail();
			break;
			//TODO: handle internal cases.
		default:
			break;
		}
	}

protected:
	const bool IsDiscovered()
	{
		return DiscoveryState == DiscoveryStateEnum::Running;
	}

private:
	static constexpr uint32_t GetTurnoverTimestamp(const uint8_t slot)
	{
		return ((DISCOVERY_SLOT_PERIOD_MICROS * (slot + 1)));
	}

	const bool StartSlot()
	{
		const uint32_t turnOvers = (LoLaLink->GetLinkElapsed() + DISCOVERY_SLOT_TOLERANCE_MICROS) / DISCOVERY_SLOT_PERIOD_MICROS;
		if (turnOvers < DISCOVERY_SLOT_COUNT)
		{
			if (LocalSlot < turnOvers + 1)
			{
				LocalSlot = turnOvers + 1;
			}
		}
		else
		{
			LocalSlot = UINT8_MAX;
		}

		return LocalSlot < DISCOVERY_SLOT_COUNT;
	}

	const bool DiscoveryTimedOut()
	{
		if (LoLaLink->GetLinkElapsed() > DISCOVERY_TIMEOUT_MICROS)
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
			RequestSendCancel();
			OnDiscoveryFailed();

			return true;
		}

		return false;
	}

	const bool RequestSendDiscovery(const bool discoveryReject, const bool slotMatched)
	{
		if (CanRequestSend())
		{
			OutPacket.SetPort(Port);
			OutPacket.SetHeader(DiscoveryDefinition::HEADER);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_META_INDEX] = DiscoveryDefinition::GetMeta(LocalSlot, discoveryReject, slotMatched);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX] = (uint8_t)ServiceId;
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 1] = (uint8_t)(ServiceId >> 8);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 2] = (uint8_t)(ServiceId >> 16);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 3] = (uint8_t)(ServiceId >> 24);

			return RequestSendPacket(DiscoveryDefinition::PAYLOAD_SIZE, RequestPriority::REGULAR);
		}

		return false;
	}


	static const uint8_t GetNextSlot(const uint8_t slotA, const uint8_t slotB)
	{
		if (slotA >= (DISCOVERY_SLOT_COUNT - 1)
			|| slotA >= (DISCOVERY_SLOT_COUNT - 1))
		{
			return DISCOVERY_SLOT_COUNT;
		}

		if (slotA > slotB)
		{
			return slotA;
		}
		else
		{
			return slotB;
		}
	}
};
#endif