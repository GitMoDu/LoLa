// AbstractDiscoveryService.h

#ifndef _ABSTRACT_DISCOVERY_SERVICE_
#define _ABSTRACT_DISCOVERY_SERVICE_

#include "..\..\Service\TemplateLoLaService.h"
#include "DiscoveryDefinitions.h"

using namespace DiscoveryDefinitions;

/// <summary>
/// Discovers and acknowledges the existance of a partner service,
/// on the same port and using the same service id.
/// Linked callbacks only after discovery is complete.
/// Can be piggybacked on by using TemplateHeaderDefinitions. 
/// Header UINT8_MAX is reserved for Discovery Service.
/// </summary>
/// <typeparam name="Port">The port registered for this service.</typeparam>
/// <typeparam name="ServiceId">Unique service identifier.</typeparam>
/// <typeparam name="MaxSendPayloadSize">The max packet payload sent by this service.</typeparam>
template<const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize = 0>
class AbstractLoLaDiscoveryService
	: public TemplateLoLaService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>
	, public virtual ILinkListener
{
private:
	using BaseClass = TemplateLoLaService<DiscoveryDefinition::MaxPayloadSize(MaxSendPayloadSize)>;

	static constexpr uint32_t RetryPeriodMicros = 30000;
	static constexpr uint32_t NoDiscoveryTimeOut = 5000;

	enum DiscoveryStateEnum
	{
		WaitingForLink,
		Discovering,
		Acknowledging,
		Running,
		RunningAck
	};

protected:
	using BaseClass::LoLaLink;
	using BaseClass::RequestSendCancel;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::RegisterPacketListener;
	using BaseClass::PacketThrottle;
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
	/// The user class can ride this task's callback while linked,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnLinkedService() { Task::disable(); }

	/// <summary>
	/// Fires when the user class's packet send request
	///  failed after transmission request.
	/// </summary>
	virtual void OnLinkedSendRequestFail() { }

	/// <summary>
	/// After discovery is completed, any non-discovery packets will be routed to the user class.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) {}

private:
	uint32_t DiscoveryStart = 0;
	DiscoveryStateEnum DiscoveryState = DiscoveryStateEnum::WaitingForLink;

	bool Acknowledged = false;

public:
	AbstractLoLaDiscoveryService(Scheduler& scheduler, ILoLaLink* loLaLink, const uint32_t sendRequestTimeout = 100)
		: BaseClass(scheduler, loLaLink, sendRequestTimeout)
	{}

public:
	virtual const bool Setup()
	{
		DiscoveryState = DiscoveryStateEnum::WaitingForLink;

		return BaseClass::Setup()
			&& LoLaLink->RegisterLinkListener(this)
			&& RegisterPacketListener(Port);
	}

	virtual void OnLinkStateUpdated(const bool hasLink) final
	{
		if (hasLink)
		{
			if (DiscoveryState == DiscoveryStateEnum::WaitingForLink)
			{
				Acknowledged = false;
				DiscoveryState = DiscoveryStateEnum::Discovering;
				DiscoveryStart = millis();
				Task::enableIfNot();
			}
		}
		else
		{
			if (DiscoveryState == DiscoveryStateEnum::Running)
			{
				OnServiceEnded();
			}
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
			Task::enableIfNot();
		}
	}

	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port != Port)
		{
			return;
		}

		if (payload[HeaderDefinition::HEADER_INDEX] == DiscoveryDefinition::HEADER)
		{
			if (payloadSize == DiscoveryDefinition::PAYLOAD_SIZE
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX] == (uint8_t)(ServiceId)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 1] == (uint8_t)(ServiceId >> 8)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 2] == (uint8_t)(ServiceId >> 16)
				&& payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 3] == (uint8_t)(ServiceId >> 24))
			{
				switch (DiscoveryState)
				{
				case DiscoveryStateEnum::Discovering:
					DiscoveryState = DiscoveryStateEnum::Acknowledging;
					Acknowledged |= payload[DiscoveryDefinition::PAYLOAD_ACK_INDEX] > 0;
					break;
				case DiscoveryStateEnum::Acknowledging:
					if (Acknowledged && payload[DiscoveryDefinition::PAYLOAD_ACK_INDEX] > 0)
					{
						// We have been acknowledged and we know it, discovery is finished.
						DiscoveryState = DiscoveryStateEnum::Running;
						Task::enableIfNot();
						OnServiceStarted();
					}
					else
					{
						Acknowledged |= payload[DiscoveryDefinition::PAYLOAD_ACK_INDEX] > 0;
					}
					break;
				case DiscoveryStateEnum::Running:
					DiscoveryState = DiscoveryStateEnum::RunningAck;
					Task::enableDelayed(0);
					break;
				default:
					// Received packet at an unexpected state.
					return;
					break;
				}
			}
		}
		else
		{
			OnLinkedPacketReceived(timestamp, payload, payloadSize);
		}
	}

protected:
	virtual void OnService() final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::WaitingForLink:
			Task::disable();
			break;
		case DiscoveryStateEnum::Discovering:
			if (millis() - DiscoveryStart > NoDiscoveryTimeOut)
			{
				// After NoDiscoveryTimeOut ms have elapsed, we give up.
				OnDiscoveryFailed();
			}
			else if (PacketThrottle(RetryPeriodMicros))
			{
				if (!RequestSendDiscovery(false))
				{
					Task::delay(0);
				}
			}
			else
			{
				Task::delay(0);
			}
			break;
		case DiscoveryStateEnum::Acknowledging:
			if (millis() - DiscoveryStart > NoDiscoveryTimeOut)
			{
				// After NoDiscoveryTimeOut ms have elapsed, we give up.
				OnDiscoveryFailed();
			}
			else if (PacketThrottle(RetryPeriodMicros))
			{
				if (!RequestSendDiscovery(true))
				{
					Task::delay(0);
				}
			}
			else
			{
				Task::delay(0);
			}
			break;
		case DiscoveryStateEnum::RunningAck:
			if (PacketThrottle(RetryPeriodMicros))
			{
				Task::delay(0);
				if (RequestSendDiscovery(true))
				{
					DiscoveryState = DiscoveryStateEnum::Running;
				}
			}
			break;
		case DiscoveryStateEnum::Running:
			OnLinkedService();
			break;
		default:
			break;
		}
	}

	virtual void OnSendRequestTimeout() final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::RunningAck:
		case DiscoveryStateEnum::Running:
			OnLinkedSendRequestFail();
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
	const bool RequestSendDiscovery(const bool partnerFound)
	{
		if (CanRequestSend())
		{
			OutPacket.SetPort(Port);
			OutPacket.SetHeader(DiscoveryDefinition::HEADER);

			if (partnerFound)
			{
				OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ACK_INDEX] = UINT8_MAX;
			}
			else
			{
				OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ACK_INDEX] = 0;
			}

			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX] = (uint8_t)ServiceId;
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 1] = (uint8_t)(ServiceId >> 8);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 2] = (uint8_t)(ServiceId >> 16);
			OutPacket.Payload[DiscoveryDefinition::PAYLOAD_ID_INDEX + 3] = (uint8_t)(ServiceId >> 24);

			return RequestSendPacket(DiscoveryDefinition::PAYLOAD_SIZE);
		}

		return false;
	}
};
#endif