// AbstractDiscoveryService.h

#ifndef _ABSTRACT_DISCOVERY_SERVICE_
#define _ABSTRACT_DISCOVERY_SERVICE_

#include "../../Services/Template/TemplateLinkService.h"
#include "DiscoveryDefinitions.h"

using namespace DiscoveryDefinitions;

/// <summary>
/// Abstract service to be extended.
/// Discovers and acknowledges the existence of a partner service,
/// on the same port and using the same service id.
/// Linked callbacks only fire after discovery is complete.
/// Can be piggybacked on by using TemplateHeaderDefinitions. 
/// Header UINT8_MAX is reserved for Discovery Service.
/// Rejects packets if not discovered, and requests partner to stop service.
/// </summary>
/// <typeparam name="Port">The port registered for this service.</typeparam>
/// <typeparam name="ServiceId">Unique service identifier.</typeparam>
/// <typeparam name="MaxSendPayloadSize">The max packet payload sent by this service.
/// Effective value will always be at least as big as DiscoveryDefinition::PAYLOAD_SIZE.</typeparam>
template<const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize = 0>
class AbstractDiscoveryService
	: public TemplateLinkService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>
	, public virtual ILinkListener
{
private:
	using BaseClass = TemplateLinkService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>;

	static constexpr uint32_t DISCOVERY_SLOT_PERIOD_MICROS = LoLaLinkDefinition::DUPLEX_PERIOD_MAX_MICROS * 5;
	static constexpr uint32_t DISCOVERY_TIMEOUT_MICROS = 10000000;
	static constexpr uint16_t DISCOVERY_SLOT_TOLERANCE_MICROS = 1000;
	static constexpr uint8_t DISCOVERY_SLOT_COUNT = DiscoveryDefinition::SLOT_MASK;

	enum class DiscoveryStateEnum : uint8_t
	{
		WaitingForLink,
		Discovering,
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
	virtual void OnServiceStarted() { TS::Task::disable(); }

	/// <summary>
	/// Fires when the the service has been lost.
	/// </summary>
	virtual void OnServiceEnded() { TS::Task::disable(); }

	/// <summary>
	/// Fires when the the service partner can't be found.
	/// </summary>
	virtual void OnDiscoveryFailed() { TS::Task::disable(); }

	/// <summary>
	/// The service can ride this task's callback while linked,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnLinkedServiceRun() { TS::Task::disable(); }

	/// <summary>
	/// After discovery is completed, any non-discovery packets will be routed to the service.
	/// </summary>
	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}

private:
	DiscoveryStateEnum DiscoveryState = DiscoveryStateEnum::WaitingForLink;
	uint8_t LocalSlot = 0;
	bool ReplyPending = false;

public:
	AbstractDiscoveryService(TS::Scheduler& scheduler, ILoLaLink* loLaLink)
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
				TS::Task::enableDelayed(1);
				DiscoveryState = DiscoveryStateEnum::Discovering;
			}
			else
			{
				TS::Task::enableIfNot();
			}
		}
		else
		{
			RequestSendCancel();
			TS::Task::disable();
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
	/// <param name="timestamp">Timestamp of the receive start.</param>
	/// <param name="payload">Pointer to payload array.</param>
	/// <param name="payloadSize">Received payload size.</param>
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
#if defined(DEBUG_LOLA)
					Serial.println(F("Discovery remote shutdown."));
#endif
					if (DiscoveryState == DiscoveryStateEnum::Running)
					{
						RequestSendCancel();
						TS::Task::disable();
						OnServiceEnded();
					}
					DiscoveryState = DiscoveryStateEnum::WaitingForLink;
				}
				else if (DiscoveryState != DiscoveryStateEnum::WaitingForLink)
				{
					const uint8_t remoteSlot = DiscoveryDefinition::GetSlotFromMeta(payload[DiscoveryDefinition::PAYLOAD_META_INDEX]);
					const bool remoteAck = DiscoveryDefinition::GetAckFromMeta(payload[DiscoveryDefinition::PAYLOAD_META_INDEX]);
					const bool slotsMatch = remoteSlot == LocalSlot;

					switch (DiscoveryState)
					{
					case DiscoveryStateEnum::Discovering:
						DiscoveryState = DiscoveryStateEnum::MatchingSlot;
						LocalSlot = GetStartSlot(remoteSlot);
						ResetPacketThrottle();
						TS::Task::delay(0);
						break;
					case DiscoveryStateEnum::MatchingSlot:
						if ((LoLaLink->GetLinkElapsed() + DISCOVERY_SLOT_TOLERANCE_MICROS) <= GetTurnoverTimestamp(LocalSlot)
							&& slotsMatch)
						{
							DiscoveryState = DiscoveryStateEnum::MatchedSlot;
						}
						LocalSlot = GetNextSlot(LocalSlot, remoteSlot);
						ResetPacketThrottle();
						break;
					case DiscoveryStateEnum::MatchedSlot:
						LocalSlot = GetNextSlot(LocalSlot, remoteSlot);
						if ((LoLaLink->GetLinkElapsed()) <= GetTurnoverTimestamp(LocalSlot)
							&& slotsMatch
							&& remoteAck)
						{
							ReplyPending = true;
							DiscoveryState = DiscoveryStateEnum::WaitingForSlotEnd;
						}
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
						}
						else
						{
							ResetPacketThrottle();
							DiscoveryState = DiscoveryStateEnum::MatchedSlot;
						}
						break;
					case DiscoveryStateEnum::Running:
					default:
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
			TS::Task::enableDelayed(0);
		}
		else
		{
			ReplyPending = true;
			TS::Task::enableDelayed(0);
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
				TS::Task::disable();
			}
			break;
		case DiscoveryStateEnum::Discovering:
			if (!DiscoveryTimedOut())
			{
				LocalSlot = GetStartSlot(GetEstimatedSlot(DISCOVERY_SLOT_TOLERANCE_MICROS));
				if (PacketThrottle())
				{
					RequestSendDiscovery(false, false);
				}
				else { TS::Task::delay(0); }
			}
			break;
		case DiscoveryStateEnum::MatchingSlot:
			if (!DiscoveryTimedOut())
			{
				TS::Task::delay(0);
				if ((LoLaLink->GetLinkElapsed()) >= GetTurnoverTimestamp(LocalSlot))
				{
					LocalSlot++;
				}
				if (PacketThrottle())
				{
					RequestSendDiscovery(false, false);
				}
			}
			break;
		case DiscoveryStateEnum::MatchedSlot:
			if (DiscoveryTimedOut())
			{
				ReplyPending = true;
			}
			else
			{
				if ((LoLaLink->GetLinkElapsed()) > GetTurnoverTimestamp(LocalSlot))
				{
					LocalSlot++;
					TS::Task::delay(0);
					ResetPacketThrottle();
				}

				if (PacketThrottle())
				{
					RequestSendDiscovery(false, true);
				}
			}
			break;
		case DiscoveryStateEnum::WaitingForSlotEnd:
			TS::Task::delay(0);
			if (ReplyPending)
			{
				if (PacketThrottle()
					&& RequestSendDiscovery(false, true))
				{
					ReplyPending = false;
				}
			}
			else if (LoLaLink->GetLinkElapsed() >= GetTurnoverTimestamp(LocalSlot))
			{
				DiscoveryState = DiscoveryStateEnum::Running;
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

protected:
	const bool IsDiscovered()
	{
		return DiscoveryState == DiscoveryStateEnum::Running;
	}

private:
	static constexpr uint32_t GetTurnoverTimestamp(const uint8_t slot)
	{
		return (uint32_t)DISCOVERY_SLOT_PERIOD_MICROS * (slot + 1);
	}

	const uint32_t GetEstimatedSlot(const int32_t offset)
	{
		return (LoLaLink->GetLinkElapsed() + offset) / DISCOVERY_SLOT_PERIOD_MICROS;
	}

	const uint8_t GetStartSlot(const uint8_t startingSlot)
	{
		const uint32_t nextTurnOver = GetEstimatedSlot(+1);
		if (nextTurnOver < DISCOVERY_SLOT_COUNT)
		{
			if (startingSlot <= nextTurnOver)
			{
				return nextTurnOver;
			}
			else
			{
				return startingSlot;
			}
		}
		else
		{
			return UINT8_MAX;
		}
	}

	const bool DiscoveryTimedOut()
	{
		if (LocalSlot >= DISCOVERY_SLOT_COUNT
			|| LoLaLink->GetLinkElapsed() > DISCOVERY_TIMEOUT_MICROS)
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
			RequestSendCancel();

#if defined(DEBUG_LOLA)
			Serial.print(F("Discovery Timed out after "));
			Serial.print(LoLaLink->GetLinkElapsed());
			Serial.print(F("us Slot = "));
			Serial.println(LocalSlot);
#endif
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
		if (slotA >= DISCOVERY_SLOT_COUNT
			|| slotB >= DISCOVERY_SLOT_COUNT)
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