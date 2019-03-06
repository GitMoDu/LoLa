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
		WaitingForSentOk = 2,
		SentOk = 3,
		SendFailed = 4,
		WaitingForAck = 5,
		AckOk = 6,
		AckFailed = 7
	} SendStatus = SendStatusEnum::Done;

	uint8_t SendFailures = 0;
	uint32_t SendStartMillis = ILOLA_INVALID_MILLIS;
	uint8_t SendTimeOutDuration = 0;
	uint8_t AckTimeOutDuration = 0;

	bool OverrideSendPermission = false;

protected:
	ILoLaPacket * Packet = nullptr;

private:
	inline bool HasSendPendingInternal()
	{
		return Packet->GetDefinition() != nullptr;
	}

protected:
	virtual void OnAckFailed(const uint8_t header, const uint8_t id) { }
	virtual void OnAckReceived(const uint8_t header, const uint8_t id) { }
	virtual void OnSendOk(const uint8_t header, const uint32_t sendDuration) { SetNextRunASAP(); }
	virtual void OnSendFailed() { SetNextRunASAP(); }
	virtual void OnService() { SetNextRunDelay(LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS); }
	virtual void OnSendTimedOut() { SetNextRunASAP(); }
	virtual void OnSendDelayed() { }
	virtual void OnSendRetrying() { }
	virtual void OnPreSend() { }
	virtual bool OnEnable() { return true; }
	virtual void OnDisable() { }
	virtual bool OnSetup()
	{
		return Packet != nullptr;
	}

public:
	IPacketSendService(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ILoLaPacket* packetHolder)
		: ILoLaService(scheduler, period, loLa)
	{
		Packet = packetHolder;
		Packet->ClearDefinition();
	}

	bool ProcessSent(const uint8_t header)
	{
		if (HasSendPendingInternal() &&
			Packet->GetDataHeader() == header &&
			SendStatus == SendStatusEnum::WaitingForSentOk)
		{
			//Notify sent Ok will be fired, as soon as the service runs.
			SendStatus = SendStatusEnum::SentOk;
			SetNextRunASAP();
			return true;
		}

		return false;
	}

	bool Callback()
	{
		//Ensure we only deal with sending if there's request pending, otherwise yeald back to main service.
		if (!HasSendPendingInternal() && SendStatus != SendStatusEnum::Done)
		{
			SendStatus = SendStatusEnum::Done;
			SetNextRunASAP();
		}
		//Finish sending any pending packet.
		switch (SendStatus)
		{
		case SendStatusEnum::SendingPacket:
			if (SendStartMillis == ILOLA_INVALID_MILLIS ||
				((millis() - SendStartMillis) > SendTimeOutDuration))
			{
				OnSendTimedOut();
				ClearSendRequest();
				break;
			}

			if (!AllowedSend(OverrideSendPermission))
			{
				//Give an opportunity for the service to update the packet, if needed.
				SetNextRunDelay(LOLA_SEND_SERVICE_CHECK_PERIOD_MILLIS);
				OnSendDelayed();
				break;
			}

			//Last minute fast stuff.
			OnPreSend();

			//SendPacket is quite time consuming.
			if (SendPacket(Packet))
			{
				SendStatus = SendStatusEnum::WaitingForSentOk;
				SetNextRunDelay(SendTimeOutDuration - constrain(millis() - SendStartMillis, 0, SendTimeOutDuration));
			}
			else
			{
				SendFailures++;
				if (SendFailures > LOLA_SEND_SERVICE_DENIED_MAX_FAILS)
				{
					SendStatus = SendStatusEnum::SendFailed;
					SetNextRunASAP();
				}
				else
				{
					SetNextRunDelay(LOLA_SEND_SERVICE_CHECK_PERIOD_MILLIS);
					OnSendRetrying();
				}
			}
			break;
		case SendStatusEnum::WaitingForSentOk:
			SendStatus = SendStatusEnum::SendFailed;
			SetNextRunASAP();
			break;
		case SendStatusEnum::SentOk:
			OnSendOk(Packet->GetDataHeader(), millis() - SendStartMillis);
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
		case SendStatusEnum::SendFailed:
			OnSendFailed();
			ClearSendRequest();
			break;
		case SendStatusEnum::WaitingForAck:
			SendStatus = SendStatusEnum::AckFailed;
			SetNextRunASAP();
			break;
		case SendStatusEnum::AckOk:
			OnAckReceived(Packet->GetDataHeader(), Packet->GetId());
			ClearSendRequest();
			break;
		case SendStatusEnum::AckFailed:
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
	void RequestSendPacket(const bool overrideSendPermission = false, const uint8_t sendTimeOutDurationMillis = LOLA_SEND_SERVICE_SEND_TIMEOUT_DEFAULT_MILLIS,
		const uint8_t ackReplyTimeOutDurationMillis = LOLA_SEND_SERVICE_REPLY_TIMEOUT_DEFAULT_MILLIS)
	{
		if (HasSendPendingInternal())
		{
			SendStartMillis = millis();
			SendTimeOutDuration = sendTimeOutDurationMillis;
			AckTimeOutDuration = ackReplyTimeOutDurationMillis;
			OverrideSendPermission = overrideSendPermission;
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
		SendStartMillis = ILOLA_INVALID_MILLIS;
		SendStatus = SendStatusEnum::Done;
		SetNextRunASAP();
	}

public:
	bool ProcessAck(const uint8_t header, const uint8_t id)
	{
		//Make sure we eat out own packets.
		if (Packet->GetDataHeader() == header)
		{
			if (SendStatus == SendStatusEnum::WaitingForAck &&
				HasSendPendingInternal())
			{
				if (Packet->GetDefinition()->HasId() &&
					Packet->GetId() != id)
				{
					return false;
				}


				//Notify Ack received will be fired, as soon as the service runs.
				SendStatus = SendStatusEnum::AckOk;
				SetNextRunASAP();
			}

			return true;
		}

		return false;
	}
};
#endif