// AbstractLoLaDiscoveryService.h

#ifndef _ABSTRACT_LOLA_DISCOVERY_SERVICE_
#define _ABSTRACT_LOLA_DISCOVERY_SERVICE_

#include "..\Service\TemplateLoLaService.h"

/// <summary>
/// </summary>
/// <param name="Port">The port registered for this service.
///  Can be piggybacked on by using the first byte of the payload as a header (excluding UINT8_MAX).</param>
template<const uint8_t Port,
	const uint8_t MaxSendPayloadSize = 3,
	const bool AutoStart = true,
	const uint32_t NoDiscoveryTimeOut = 30000>
class AbstractLoLaDiscoveryService
	: public TemplateLoLaService<MaxSendPayloadSize>
	, public virtual ILinkListener
{
private:
	using BaseClass = TemplateLoLaService<MaxSendPayloadSize>;

	/// <summary>
	/// Discovery sub header start at the top, leaving the bottom free.
	/// </summary>
	class DiscoverySubDefinition : public TemplateSubHeaderDefinition<UINT8_MAX, 2>
	{
	public:
		static const uint8_t PAYLOAD_ID_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		static const uint8_t PAYLOAD_ACK_INDEX = PAYLOAD_ID_INDEX + 1;
	};

protected:
	using BaseClass::LoLaLink;
	using BaseClass::RegisterPort;
	using BaseClass::RequestSendCancel;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetLastSent;
	using BaseClass::GetElapsedSinceLastSent;

	using SendResultEnum = ILinkPacketSender::SendResultEnum;

protected:
	TemplateLoLaOutDataPacket<MaxSendPayloadSize> OutPacket;

private:
	enum DiscoveryStateEnum
	{
		WaitingForManualActivation,
		WaitingForLink,
		SendingSearch,
		WaitingForReply,
		Ready,
		Running
	};

	static const uint32_t RetryPeriod = 2;
	static const uint32_t SendNoResponseTimeoutPeriod = 50;

	uint32_t DiscoveryStart = 0;

	DiscoveryStateEnum DiscoveryState = DiscoveryStateEnum::WaitingForManualActivation;
	bool Acknowledged = false;

protected:
	/// <summary>
	/// Service content identifier. Double checks that the registered service on the other is has the same abstract "content".
	/// </summary>
	/// <returns></returns>
	virtual const uint8_t GetServiceId() { return 0; }


	/// <summary>
	/// Fires when the the service has been discovered.
	/// </summary>
	virtual void OnServiceStarted() { Task::disable(); }

	/// <summary>
	/// Fires when the the service has been lost.
	/// </summary>
	virtual void OnServiceEnded() { Task::disable(); }

	/// <summary>
	/// The user class can ride this task's callback,
	/// once the discovery has been complete.
	/// </summary>
	virtual void OnDiscoveredService() { Task::disable(); }


	/// <summary>
	/// Fires when the user class's packet send request
	///  failed after transmission request.
	/// </summary>
	/// <param name="port">The requested sent port.</param>
	virtual void OnDiscoveredSendRequestFail(const uint8_t port) { }


	/// <summary>
	/// 
	/// </summary>
	/// <param name="startTimestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	virtual void OnDiscoveredPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize) {}

public:
	AbstractLoLaDiscoveryService(Scheduler& scheduler, ILoLaLink* loLaLink, const uint32_t sendRequestTimeout = 100)
		: BaseClass(scheduler, loLaLink, sendRequestTimeout)
	{
	}

public:
	virtual const bool Setup()
	{
		if (AutoStart)
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
		}
		else
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForManualActivation;
		}

		return BaseClass::Setup() &&
			LoLaLink->RegisterLinkListener(this) &&
			RegisterPort(Port);
	}

	virtual void OnLinkStateUpdated(const bool hasLink) final
	{
		if (hasLink)
		{
			if (DiscoveryState == DiscoveryStateEnum::WaitingForLink)
			{
				StartInternal();
			}
		}
		else
		{
			StopDiscovery();
		}
	}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port == Port)
		{
			const uint8_t subHeader = payload[SubHeaderDefinition::SUB_HEADER_INDEX];
			if (subHeader == DiscoverySubDefinition::SUB_HEADER) {
				if (payloadSize == DiscoverySubDefinition::PAYLOAD_SIZE &&
					payload[DiscoverySubDefinition::ID_OFFSET] == GetServiceId()) {
					switch (DiscoveryState)
					{
					case DiscoveryStateEnum::SendingSearch:
						RequestSendCancel();
						DiscoveryState = DiscoveryStateEnum::Ready;
						UpdateAcknowledge(payload[DiscoverySubDefinition::ACK_OFFSET] > 0);
						Task::enableIfNot();
						break;
					case DiscoveryStateEnum::WaitingForLink:
						break;
					case DiscoveryStateEnum::WaitingForReply:
						DiscoveryState = DiscoveryStateEnum::Ready;
						UpdateAcknowledge(payload[DiscoverySubDefinition::ACK_OFFSET] > 0);
						Task::enableIfNot();
						break;
					case DiscoveryStateEnum::Ready:
						UpdateAcknowledge(payload[DiscoverySubDefinition::ACK_OFFSET] > 0);
						Task::enableIfNot();
					case DiscoveryStateEnum::WaitingForManualActivation:
						// Host has not enabled the service.
						break;
					default:
						// Acknowledged is not updated on an unexpected state.
						break;
					}
				}
			}
			else
			{
				OnDiscoveredPacketReceived(startTimestamp, payload, payloadSize);
			}
		}
	}

protected:
	/// <summary>
	/// Starts discovery immediatelly if Link is available.
	/// Otherwise sets up the state so it will wake-up when the Link is available.
	/// </summary>
	void StartDiscovery()
	{
		if (LoLaLink->HasLink())
		{
			StartInternal();
		}
		else
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
			Task::enableIfNot();
		}
	}

	void StopDiscovery()
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::Running:
			OnServiceEnded();
			break;
		default:
			break;
		}
		if (AutoStart)
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForLink;
		}
		else
		{
			DiscoveryState = DiscoveryStateEnum::WaitingForManualActivation;
		}
		Task::enableIfNot();
	}

	virtual void OnService() final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::WaitingForLink:
			Task::disable();
			break;
		case DiscoveryStateEnum::WaitingForManualActivation:
			Task::disable();
			break;
		case DiscoveryStateEnum::SendingSearch:
			if (RequestSendDiscovery())
			{
				DiscoveryState = DiscoveryStateEnum::WaitingForReply;
			}
			else
			{
				Task::delay(RetryPeriod);
			}
			break;
		case DiscoveryStateEnum::WaitingForReply:
			if (millis() - DiscoveryStart > NoDiscoveryTimeOut)
			{
				// After NoDiscoveryTimeOut ms have elapsed, we give up?
				Task::disable();
			}
			else if (GetElapsedSinceLastSent() > SendNoResponseTimeoutPeriod)
			{
				DiscoveryState = DiscoveryStateEnum::SendingSearch;
				Task::enable();
			}
			else
			{
				Task::delay(RetryPeriod);
			}
			break;
		case DiscoveryStateEnum::Ready:
			if (Acknowledged)
			{
				DiscoveryState = DiscoveryStateEnum::Running;
				OnServiceStarted();
			}
			else
			{
				DiscoveryState = DiscoveryStateEnum::SendingSearch;
			}
			break;
		case DiscoveryStateEnum::Running:
			OnDiscoveredService();
		default:
			break;
		}
	}

	virtual void OnSendRequestFail(const uint8_t port) final
	{
		switch (DiscoveryState)
		{
		case DiscoveryStateEnum::WaitingForReply:
			DiscoveryState = DiscoveryStateEnum::SendingSearch;
		case DiscoveryStateEnum::SendingSearch:
			ResetLastSent();
			break;
		case DiscoveryStateEnum::Running:
			OnDiscoveredSendRequestFail(port);
			break;
		default:
			break;
		}
	}

private:
	void StartInternal(const bool startingAckStatus = false)
	{
		Acknowledged = startingAckStatus;
		DiscoveryState = DiscoveryStateEnum::SendingSearch;
		DiscoveryStart = millis();
		Task::enableIfNot();
	}

	void UpdateAcknowledge(const bool ack)
	{
		// Update remote acknowledged state.
		if (!Acknowledged && ack)
		{
			Acknowledged = true;
		}
	}

	const bool RequestSendDiscovery()
	{
		OutPacket.SetPort(Port);

		OutPacket.Payload[SubHeaderDefinition::SUB_HEADER_INDEX] = DiscoverySubDefinition::SUB_HEADER;
		OutPacket.Payload[DiscoverySubDefinition::ID_OFFSET] = GetServiceId();
		if (DiscoveryState == DiscoveryStateEnum::Ready)
		{
			OutPacket.Payload[DiscoverySubDefinition::ACK_OFFSET] = UINT8_MAX;
		}
		else
		{
			OutPacket.Payload[DiscoverySubDefinition::ACK_OFFSET] = 0;
		}

		return RequestSendPacket(DiscoverySubDefinition::PAYLOAD_SIZE);
	}
};
#endif