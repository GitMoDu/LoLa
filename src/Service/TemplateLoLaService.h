// TemplateLoLaService.h

#ifndef _TEMPLATE_LOLA_SERVICE_
#define _TEMPLATE_LOLA_SERVICE_

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\Link\ILoLaLink.h"
#include "..\Link\LoLaPacketDefinition.h"

template<const uint8_t MaxSendPayloadSize>
class TemplateLoLaService : protected Task
	, public virtual ILinkPacketReceiver
	, public virtual ILinkPacketSender
{
private:
	using ILinkPacketSender::SendResultEnum;

protected:
	/// <summary>
	/// Output packet payload, to be used by service to send packets.
	/// </summary>
	TemplateLoLaOutDataPacket<MaxSendPayloadSize> OutPacket;

	ILoLaLink* LoLaLink;

private:
	uint32_t SendRequestTimeout = 0;
	uint32_t RequestStart = 0;
	uint32_t LastRequestSent = 0;

	uint8_t RequestPayloadSize = 0;
	uint8_t SendHandle = 0;

	bool RequestPending = 0;

protected:
	/// <summary>
	/// Last second opportunity to update payload.
	/// To be overriden by user class.
	/// </summary>
	/// <param name="handle">Packet request id handle.</param>
	virtual void OnPreSend(const uint8_t handle) { }

	/// <summary>
	/// The requested packet to send has failed.
	/// To be overriden by user class.
	/// </summary>
	/// <param name="handle">Packet request id handle.</param>
	virtual void OnSendRequestFail(const uint8_t handle) { }

	/// <summary>
	/// The user class can ride this task's callback, when no sending is being performed.
	/// Useful for in-line services (expected to block flow until send is complete).
	/// </summary>
	/// <param name="handle">Packet request id handle.</param>
	virtual void OnService() { }


	/// <summary>
	/// Exposed Link and packet callbacks.
	/// To be overriden by user class.
	/// </summary>
public:
	virtual void OnLinkStateUpdated(const bool hasLink) {}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}

public:
	TemplateLoLaService(Scheduler& scheduler, ILoLaLink* loLaLink, const uint32_t sendRequestTimeout = 100)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILinkPacketReceiver()
		, ILinkPacketSender()
		, LoLaLink(loLaLink)
	{
		SetSendRequestTimeout(sendRequestTimeout);
	}

	virtual const bool Setup()
	{
		return LoLaLink != nullptr;
	}

	virtual bool Callback() final
	{
		if (RequestPending)
		{
			if (LoLaLink->CanSendPacket(RequestPayloadSize))
			{
				OnPreSend(SendHandle);

				if (LoLaLink->SendPacket(this, SendHandle, OutPacket.Data, RequestPayloadSize))
				{
					RequestPending = false;
					LastRequestSent = millis();
				}
				else
				{
					// Unable to transmit, try again until timeout.
				}
				Task::enable();
			}
			else if (millis() - RequestStart > SendRequestTimeout)
			{
				RequestPending = false;
				OnSendRequestFail(OutPacket.GetPort());
			}
			else
			{
				// Can't send now, try again until timeout.
				Task::enableIfNot();
			}
		}
		else
		{
			OnService();
		}

		return true;
	}

protected:
	const uint32_t GetElapsedSinceLastSent()
	{
		return millis() - LastRequestSent;
	}

	void RequestSendCancel()
	{
		RequestPending = false;
		Task::enableIfNot();
	}

	const bool CanRequestSend()
	{
		return !RequestPending;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>False if a previous send request was interrupted.</returns>
	const bool RequestSendPacket(const uint8_t payloadSize)
	{
		if (RequestPending)
		{
			// Interrupted another request.
			return false;
		}

		RequestPending = true;
		RequestPayloadSize = payloadSize;
		RequestStart = millis();

#if defined(DEBUG_LOLA)
		if (payloadSize <= MaxSendPayloadSize)
		{
			Task::enable();
			return true;
		}
		else
		{
			return false;
		}
#else
		Task::enable();
		return true;
#endif
	}

	const bool RegisterPort(const uint8_t port)
	{
		return LoLaLink->RegisterPacketReceiver(this, port);
	}

	void SetSendRequestTimeout(const uint32_t sendRequestTimeout)
	{
		if (sendRequestTimeout > 1)
		{
			SendRequestTimeout = sendRequestTimeout;
		}
		else
		{
			SendRequestTimeout = 1;
		}
	}
};
#endif