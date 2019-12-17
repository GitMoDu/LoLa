// IPacketSendService.h

#ifndef _IPACKETSENDSERVICE_h
#define _IPACKETSENDSERVICE_h


#include <Arduino.h>
#include <Services\ILoLaService.h>
#include <Packet\LoLaPacket.h>

//Takes around the same time as a full time out.
#define LOLA_SEND_SERVICE_DELAYED_MAX_DUPLEX				10 // [2 ; UINT8_MAX]
#define LOLA_SEND_SERVICE_DENIED_MAX_FAILS					3


#define LOLA_SEND_SERVICE_CHECK_PERIOD_MILLIS				(uint32_t)1
#define LOLA_SEND_SERVICE_SEND_TIMEOUT_DEFAULT_MILLIS		(uint8_t)(LOLA_SEND_SERVICE_DELAYED_MAX_DUPLEX*ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS)
#define LOLA_SEND_SERVICE_REPLY_TIMEOUT_DEFAULT_MILLIS		(uint8_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS)

#define LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS	(uint32_t)200

class IPacketSendService : public ILoLaService
{
private:
	enum SendStatusEnum : uint8_t
	{
		Done = 0,
		SendingPacket = 1,
		WaitingForSendOk = 2,
		SendOk = 3,
		WaitingForAck = 4
	} SendStatus = SendStatusEnum::Done;

	uint8_t SendFailures = 0;
	uint32_t SendStartMillis = 0;
	uint8_t SendTimeOutDuration = 0;
	uint8_t AckTimeOutDuration = 0;

protected:
	ILoLaPacket* Packet = nullptr;

private:
	inline bool HasSendPendingInternal()
	{
		return Packet->GetDefinition() != nullptr;
	}

protected:
	virtual void OnAckFailed(const uint8_t header, const uint8_t id) { }
	virtual void OnAckOk(const uint8_t header, const uint8_t id, const uint32_t timestamp) { }
	virtual void OnTransmitted(const uint8_t header, const uint8_t id, const uint32_t transmitTimestamp, const uint32_t sendDuration) { SetNextRunASAP(); }
	virtual void OnSendFailed() { SetNextRunASAP(); }
	virtual void OnService() { SetNextRunDelay(LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS); }
	virtual void OnSendTimedOut() { SetNextRunASAP(); }
	virtual void OnSendDelayed() { }
	virtual void OnSendRetrying() { SetNextRunDelay(LOLA_SEND_SERVICE_CHECK_PERIOD_MILLIS); }
	virtual void OnPreSend() { }
	virtual bool OnEnable() { return true; }
	virtual void OnDisable() { }


public:
	IPacketSendService(Scheduler* scheduler, const uint32_t period, ILoLaDriver* driver, ILoLaPacket* packetHolder)
		: ILoLaService(scheduler, period, driver)
	{
		Packet = packetHolder;
		Packet->ClearDefinition();
	}

public:
	virtual bool OnAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		// Make sure we only eat our own packets.
		if (SendStatus == SendStatusEnum::WaitingForAck &&
			Packet->GetDataHeader() == header &&
			Packet->GetId() == id)
		{
			OnAckOk(header, id, timestamp);
			ClearSendRequest();

			return true;
		}

		return false;
	}

	virtual bool Setup()
	{
		return ILoLaService::Setup() && Packet != nullptr;
	}

	virtual bool OnPacketTransmitted(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (SendStatus == SendStatusEnum::WaitingForSendOk &&
			Packet->GetDataHeader() == header)
		{
			OnTransmitted(Packet->GetDataHeader(), Packet->GetId(), timestamp, millis() - SendStartMillis);
			SendStatus = SendStatusEnum::SendOk;
			SetNextRunASAP();
			return true;
		}

		return false;
	}

	virtual bool Callback()
	{
		// Ensure we only deal with sending if there's request pending, otherwise yield back to main service.
		if (!HasSendPendingInternal() && SendStatus != SendStatusEnum::Done)
		{
			SendStatus = SendStatusEnum::Done;
			SetNextRunASAP();
		}

		// Finish sending any pending packet.
		switch (SendStatus)
		{
		case SendStatusEnum::SendingPacket:
			if (SendStartMillis == 0 ||
				((millis() - SendStartMillis) > SendTimeOutDuration))
			{
				OnSendTimedOut();
				ClearSendRequest();
				break;// Early break from case.
			}

			if (!LoLaDriver->AllowedSend())
			{
				// Give an opportunity for the service to update the packet, if needed.
				SetNextRunDelay(LOLA_SEND_SERVICE_CHECK_PERIOD_MILLIS);
				OnSendDelayed();
				break;// Early break from case.
			}

			// Last minute fast stuff.
			OnPreSend();

			// SendPacket is quite time consuming.
			// Update: driver has been severly optimized, test new times.
			if (LoLaDriver->SendPacket(Packet))
			{
				SendStatus = SendStatusEnum::WaitingForSendOk;
				SetNextRunDelay(SendTimeOutDuration - constrain(millis() - SendStartMillis, 0, SendTimeOutDuration));
			}
			else
			{
				SendFailures++;
				if (SendFailures > LOLA_SEND_SERVICE_DENIED_MAX_FAILS)
				{
					OnSendFailed();
					ClearSendRequest();
					SetNextRunASAP();
				}
				else
				{
					OnSendRetrying();
				}
			}
			break;
		case SendStatusEnum::WaitingForSendOk:
			OnSendFailed();
			ClearSendRequest();
			SetNextRunASAP();
			break;
		case SendStatusEnum::SendOk:
			if (Packet->GetDefinition()->HasACK())
			{
				SendStatus = SendStatusEnum::WaitingForAck;
				SetNextRunDelay(AckTimeOutDuration);
			}
			else
			{
				ClearSendRequest();
			}
			break;
		case SendStatusEnum::WaitingForAck:
			OnAckFailed(Packet->GetDataHeader(), Packet->GetId());
			ClearSendRequest();
			break;
		case SendStatusEnum::Done:
		default:
			OnService();
			break;
		}

		return true;
	}

protected:
	void RequestSendPacket(const uint8_t sendTimeOutDurationMillis = LOLA_SEND_SERVICE_SEND_TIMEOUT_DEFAULT_MILLIS,
		const uint8_t ackReplyTimeOutDurationMillis = LOLA_SEND_SERVICE_REPLY_TIMEOUT_DEFAULT_MILLIS)
	{
		if (HasSendPendingInternal())
		{
			SendStartMillis = millis();
			SendTimeOutDuration = sendTimeOutDurationMillis;
			AckTimeOutDuration = ackReplyTimeOutDurationMillis;
			SendFailures = 0;
			SendStatus = SendStatusEnum::SendingPacket;
		}

		SetNextRunASAP();
	}

	bool HasSendPending()
	{
		return HasSendPendingInternal();
	}

	void ClearSendRequest()
	{
		Packet->ClearDefinition();
		SendStartMillis = 0;
		SendStatus = SendStatusEnum::Done;
		SetNextRunASAP();
	}


};
#endif