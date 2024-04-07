// TemplateLinkService.h

#ifndef _TEMPLATE_LINK_SERVICE_
#define _TEMPLATE_LINK_SERVICE_

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "RequestPriority.h"

#include "..\..\Link\LoLaPacketDefinition.h"
#include "..\..\Link\ILoLaLink.h"


/// <summary>
/// Template class for all link-based services.
/// Features:
///  - Async Send, blocking ServiceRun until transmition is done.
///  - Callbacks for last chance Pre-Send, and Send-Failure.
///  - Priority handling, based on link congestion.
/// </summary>
/// <typeparam name="MaxSendPayloadSize"></typeparam>
template<const uint8_t MaxSendPayloadSize>
class TemplateLinkService : protected Task
	, public virtual ILinkPacketListener
{
private:
	static constexpr uint32_t REQUEST_TIMEOUT_MILLIS = 1000;
	static constexpr uint32_t REQUEST_TIMEOUT_MICROS = 1000 * REQUEST_TIMEOUT_MILLIS;

private:
	static constexpr uint16_t PRIORITY_NEGATIVE_SCALE = 275;
	static constexpr uint8_t PRIORITY_POSITIVE_SCALE = 15;
	static constexpr uint8_t PRIORITY_PERIOD_SCALE = 2;

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

	uint8_t PayloadSize = 0;
	uint8_t Priority = 0;

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
	virtual void OnServiceRun() { Task::disable(); }

public:
	/// <summary>
	/// Packet receive callback, to be overriden by service.
	/// Any Task manipulation other than Task::enable() will affect the send process.
	/// </summary>
	/// <param name="timestamp">Timestamp of the receive start.</param>
	/// <param name="payload">Pointer to payload array.</param>
	/// <param name="payloadSize">Received payload size.</param>
	/// <param name="port">Which registered port was the packet sent to.</param>
	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}

public:
	TemplateLinkService(Scheduler& scheduler, ILoLaLink* loLaLink)
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
		if (PayloadSize > 0)
		{
			Task::delay(0);
			if (Priority == 0)
			{
				// Busy loop waiting for send availability.
				LOLA_RTOS_PAUSE();
				if (LoLaLink->CanSendPacket(PayloadSize))
				{
					// Send is available, last moment callback before transmission.
					OnPreSend();

					// Transmit packet.
					const bool sent = LoLaLink->SendPacket(OutPacket.Data, PayloadSize);
					LOLA_RTOS_RESUME();

					if (sent)
					{
						PayloadSize = 0;
						LastSent = micros();
					}
					else if ((micros() - RequestStart) >= REQUEST_TIMEOUT_MICROS)
					{
						PayloadSize = 0;
#if defined(DEBUG_LOLA)
						Serial.println(F("Tx Failed Timeout."));
#endif
						OnSendRequestTimeout();
					}
					else
					{
						// Transmit failed on Transceiver, try again until time-out.
#if defined(DEBUG_LOLA)
						Serial.println(F("Tx Failed."));
#endif
					}
				}
				else
				{
					LOLA_RTOS_RESUME();
					// Can't send now, try again until timeout.
					if (micros() - RequestStart >= REQUEST_TIMEOUT_MICROS)
					{
						// Send timed out.
						PayloadSize = 0;
#if defined(DEBUG_LOLA)
						Serial.println(F("Tx Timeout."));
#endif
						OnSendRequestTimeout();
					}
				}
			}
			else if (GetPriorityScore(LoLaLink->GetSendElapsed(), micros() - RequestStart) >= Priority)
			{
				Priority = 0;
			}
		}
		else
		{
			OnServiceRun();
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
	/// <param name="minPeriodMicros">Lower bound period.</param>
	/// <returns>True if enough time has elapsed since the last send, for the partner to respond.</returns>
	const bool PacketThrottle(const uint32_t minPeriodMicros = 0)
	{
		const uint32_t elapsed = micros() - LastSent;

		return CanRequestSend()
			&& elapsed > LoLaLink->GetPacketThrottlePeriod()
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
		PayloadSize = 0;
	}

	/// <summary>
	/// </summary>
	/// <returns>True if no request is pending.</returns>
	const bool CanRequestSend() const
	{
		return PayloadSize == 0;
	}

	/// <summary>
	/// Request to send the current Outpacket as soon as possible.
	/// Locks service callbacks until the OutPacket is sent.
	/// </summary>
	/// <param name="payloadSize">Payload size of the current Outpacket.</param>
	/// <param name="priority"></param>
	/// <returns>False if a previous send request was interrupted.</returns>
	const bool RequestSendPacket(const uint8_t payloadSize, const RequestPriority priority = RequestPriority::REGULAR)
	{
		return RequestSendPacket(payloadSize, (const uint8_t)priority);
	}

	/// <summary>
	/// Same as RequestSendPacket(RequestPriority) but with raw priority value.
	/// </summary>
	/// <param name="payloadSize">Payload size of the current Outpacket.</param>
	/// <param name="priority">For reference values see RequestPriority.</param>
	/// <returns>False if a previous send request was interrupted.</returns>
	const bool RequestSendPacket(const uint8_t payloadSize, const uint8_t priority)
	{
		if (PayloadSize > 0)
		{
			// Attempted to interrupt another request.
#if defined(DEBUG_LOLA)
			Serial.println(F("Request attempted to interrupt a send."));
#endif
			return false;
		}

#if defined(DEBUG_LOLA)
		if (payloadSize > MaxSendPayloadSize)
		{
			return false;
		}
#endif

		RequestStart = micros();
		PayloadSize = payloadSize;
		Priority = priority;
		Task::enableDelayed(0);

		return true;
	}

protected:
	/// <summary>
	/// Scales a priority range, provided a progress.
	/// </summary>
	/// <typeparam name="PriorityMax">Target highest priority level[0;UINT8_MAX].</typeparam>
	/// <typeparam name="PriorityMin">Target lowest priority level[0;UINT8_MAX].</typeparam>
	/// <param name="progress">Scale how close to the max priority [0;UINT8_MAX].</param>
	/// <returns>Progress scaled priority between PriorityMin and PriorityMax [0;UINT8_MAX].</returns>
	template<const uint8_t PriorityMin,
		const uint8_t PriorityMax>
	static constexpr uint8_t GetProgressPriority(const uint8_t progress)
	{
		return (((uint16_t)(UINT8_MAX - progress) * PriorityMin) / UINT8_MAX)
			+ (((uint16_t)progress * PriorityMax) / UINT8_MAX);
	}

	/// <summary>
	/// Scales a priority range, provided a progress.
	/// </summary>
	/// <typeparam name="PriorityMax">Target highest priority level.</typeparam>
	/// <typeparam name="PriorityMin">Target lowest priority level.</typeparam>
	/// <param name="progress">Scale how close to the max priority [0;UINT8_MAX].</param>
	/// <returns>Progress scaled priority between PriorityMin and PriorityMax.</returns>
	template<const RequestPriority PriorityMin,
		const RequestPriority PriorityMax>
	static constexpr RequestPriority GetProgressPriority(const uint8_t progress)
	{
		return (const RequestPriority)GetProgressPriority<(const uint8_t)PriorityMin, (const uint8_t)PriorityMax>(progress);
	}

private:
	/// <summary>
	/// Calculates the priority score, to match with the request priority.
	/// </summary>
	/// <param name="txElapsed">Elapsed since last transmition ended, in microseconds.</param>
	/// <param name="requestElapsed">Elapsed since the Send Request started, in microseconds.</param>
	/// <returns>Abstract priority Score.</returns>
	const uint8_t GetPriorityScore(const uint32_t txElapsed, const uint32_t requestElapsed)
	{
		const uint32_t priorityPeriod = LoLaLink->GetPacketThrottlePeriod() * PRIORITY_PERIOD_SCALE;
		const uint32_t scoreRequest = requestElapsed / ((uint32_t)1 + (priorityPeriod / PRIORITY_NEGATIVE_SCALE));
		const uint32_t scoreTx = txElapsed / ((uint32_t)1 + (priorityPeriod / PRIORITY_POSITIVE_SCALE));

		if (scoreRequest >= UINT8_MAX)
		{
			return UINT8_MAX;
		}
		else if (scoreTx >= UINT8_MAX
			|| scoreTx >= ((uint32_t)UINT8_MAX - scoreRequest))
		{
			return UINT8_MAX;
		}
		else
		{
			return ((uint8_t)scoreRequest) + ((uint8_t)scoreTx);
		}
	}
};
#endif