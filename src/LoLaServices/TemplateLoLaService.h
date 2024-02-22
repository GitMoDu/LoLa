// TemplateLoLaService.h

#ifndef _TEMPLATE_LOLA_SERVICE_
#define _TEMPLATE_LOLA_SERVICE_

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\Link\ILoLaLink.h"
#include "..\Link\LoLaPacketDefinition.h"

/// <summary>
/// Template class for all link-based services.
/// </summary>
/// <typeparam name="MaxSendPayloadSize"></typeparam>
template<const uint8_t MaxSendPayloadSize>
class TemplateLoLaService : protected Task
	, public virtual ILinkPacketListener
{
private:
	static constexpr uint32_t REQUEST_TIMEOUT_MILLIS = 1000;

protected:
	/// <summary>
	/// Output packet payload, to be used by service to send packets.
	/// </summary>
	TemplateLoLaOutDataPacket<MaxSendPayloadSize> OutPacket{};

	/// <summary>
	/// Link source.
	/// </summary>
	ILoLaLink* LoLaLink;

private:
	uint32_t RequestStart = 0;
	uint32_t LastSent = 0;

	uint8_t RequestPayloadSize = 0;
	bool RequestPending = 0;

protected:
	/// <summary>
	/// Last chance to update payload right before transmission.
	/// </summary>
	virtual void OnPreSend() { }

	/// <summary>
	/// The requested packet to send has failed.
	/// </summary>
	virtual void OnSendRequestTimeout() { }

	/// <summary>
	/// The service class can ride this task's callback, when no send is being performed.
	/// Useful for in-line services (expected to block flow until send is complete).
	/// </summary>
	virtual void OnService() { }

public:
	/// <summary>
	/// Link state update callback.
	/// </summary>
	/// <param name="hasLink">True has been acquired, false if lost. Same value as ILoLaLink::HasLink()</param>
	virtual void OnLinkStateUpdated(const bool hasLink) {}

	/// <summary>
	/// Packet receive callback.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="port"></param>
	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}

public:
	TemplateLoLaService(Scheduler& scheduler, ILoLaLink* loLaLink)
		: ILinkPacketListener()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, LoLaLink(loLaLink)
	{}

	virtual const bool Setup()
	{
		return LoLaLink != nullptr;
	}

	virtual bool Callback() final
	{
		if (RequestPending)
		{
			// Busy loop waiting for send availability.
			Task::delay(0);

			LOLA_RTOS_PAUSE();
			if (LoLaLink->CanSendPacket(RequestPayloadSize))
			{
				// Send is available, last moment callback before transmission.
				OnPreSend();

				// Transmit packet.
				const bool sent = LoLaLink->SendPacket(OutPacket.Data, RequestPayloadSize);
				LOLA_RTOS_RESUME();

				if (sent)
				{
					LastSent = micros();
					RequestPending = false;
				}
				else
				{
					// Transmit failed.
					RequestPending = false;
					OnSendRequestTimeout();
				}
			}
			else
			{
				LOLA_RTOS_RESUME();
				// Can't send now, try again until timeout.
				if (millis() - RequestStart > REQUEST_TIMEOUT_MILLIS)
				{
					// Send timed out.
					RequestPending = false;
					OnSendRequestTimeout();
				}
			}
		}
		else
		{
			OnService();
		}

		return true;
	}

protected:
	/// <summary>
	/// Shortcut for services to register Port listeners.
	/// A service can register to multiple ports.
	/// Also checks if requested port is not reserved.
	/// </summary>
	/// <param name="port">Port number to register [0;LoLaLinkDefinition::LINK_PORT-1].</param>
	/// <returns>True if success. False if no more slots are available or Port was reserved.</returns>
	const bool RegisterPacketListener(const uint8_t port)
	{
		if (port < LoLaLinkDefinition::LINK_PORT)
		{
			return LoLaLink->RegisterPacketListener(this, port);
		}

		return false;
	}

	/// <summary>
	/// Throttles sends based on last send and link's expected response time.
	/// </summary>
	/// <returns>True if enough time has elapsed since the last send, for the partner to respond.</returns>
	const bool PacketThrottle()
	{
		return CanRequestSend()
			&& ((micros() - LastSent) > LoLaLink->GetPacketThrottlePeriod());
	}

	/// <summary>
	/// Same as PacketThrottle() but with an extra lower bound.
	/// </summary>
	/// <param name="minPeriodMicros"></param>
	/// <returns>True if enough time has elapsed since the last send, for the partner to respond.</returns>
	const bool PacketThrottle(const uint32_t minPeriodMicros)
	{
		const uint32_t elapsed = micros() - LastSent;

		return elapsed > LoLaLink->GetPacketThrottlePeriod()
			&& elapsed > minPeriodMicros;
	}

	/// <summary>
	/// Reset the thottle to this moment.
	/// </summary>
	void ResetPacketThrottle()
	{
		LastSent = micros() - REQUEST_TIMEOUT_MILLIS;
	}

	/// <summary>
	/// Cancel send request if pending and unlock service.
	/// </summary>
	void RequestSendCancel()
	{
		RequestPending = false;
		Task::enableIfNot();
	}

	/// <summary>
	/// </summary>
	/// <returns>True if no request is pending.</returns>
	const bool CanRequestSend()
	{
		return !RequestPending;
	}

	/// <summary>
	/// Locks service until the OutPacket is sent.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>False if a previous send request was interrupted.</returns>
	const bool RequestSendPacket(const uint8_t payloadSize)
	{
		if (RequestPending)
		{
			// Attempted to interrupt another request.
			return false;
		}

#if defined(DEBUG_LOLA)
		if (payloadSize > MaxSendPayloadSize)
		{
			return false;
		}
#endif

		RequestPending = true;
		RequestPayloadSize = payloadSize;
		RequestStart = millis();

		Task::enableDelayed(0);
		return true;
	}

};
#endif