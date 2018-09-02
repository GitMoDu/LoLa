// IPacketSendService.h

#ifndef _IPACKETSENDSERVICE_h
#define _IPACKETSENDSERVICE_h


#include <Arduino.h>
#include <Services\ILoLaService.h>
#include <Packet\LoLaPacket.h>

//#define DEBUG_PACKET_SERVICE

//Takes around the same time as a full time out.
#define LOLA_SEND_SERVICE_DENIED_BACK_OFF_DURATION_MILLIS	(uint32_t)2
#define LOLA_SEND_SERVICE_DENIED_MAX_FAILS					3

#define LOLA_SEND_SERVICE_SEND_TIMEOUT_DEFAULT_MILLIS		(uint32_t)500
#define LOLA_SEND_SERVICE_REPLY_TIMEOUT_DEFAULT_MILLIS		(uint32_t)300

#define LOLA_SEND_SERVICE_REPLY_DEFAULT_TIMEOUT_MILLIS		(uint32_t)(LOLA_SEND_SERVICE_SEND_TIMEOUT_DEFAULT_MILLIS+LOLA_SEND_SERVICE_REPLY_TIMEOUT_DEFAULT_MILLIS)

#define LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS	(uint32_t)200

class IPacketSendService : public ILoLaService
{
private:
	enum SendStatusEnum
	{
		Requested,
		SendingPacket,
		SentOk,
		SendFailed,
		WaitingForAck,
		AckOk,
		AckFailed,
		Done
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
	virtual void OnSendOk(const uint32_t sendDuration) { SetNextRunASAP(); }
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
		//TODO: Get comms entropy source, abstracted.
		//randomSeed(...);
		return Packet != nullptr;
	}

public:
	IPacketSendService(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ILoLaPacket* packetHolder)
		: ILoLaService(scheduler, period, loLa)
	{
		Packet = packetHolder;
		Packet->ClearDefinition();
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
		case SendStatusEnum::Requested:
			SendFailures = 0;
			SendStatus = SendStatusEnum::SendingPacket;
			break;
		case SendStatusEnum::SendingPacket:
			if (SendStartMillis == ILOLA_INVALID_MILLIS ||
				((Millis() - SendStartMillis) > SendTimeOutDuration))
			{				
				OnSendTimedOut();
#ifdef DEBUG_PACKET_SERVICE
				Serial.print(F("OnSendTimedOut:"));
				if (Packet != nullptr && Packet->GetDefinition() != nullptr)
					Serial.println(Packet->GetDefinition()->GetHeader());
				else
					Serial.println(0);
#endif
				ClearSendRequest();
			}
			else if (!AllowedSend(OverrideSendPermission))
			{
				//Give an opportunity for the service to update the packet, if needed.
				SetNextRunDelay(LOLA_SEND_SERVICE_DENIED_BACK_OFF_DURATION_MILLIS);
				OnSendDelayed();
			}
			else
			{
				//Last minute fast stuff.
				OnPreSend();

				//We only fire the OnSend and OnSendFailed events in the next cycle.
				//Because SendPacket() is already quite time consuming.
				if (SendPacket(Packet))
				{
					SendStatus = SendStatusEnum::SentOk;
					SetNextRunASAP();
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
						SetNextRunDelay(LOLA_SEND_SERVICE_DENIED_BACK_OFF_DURATION_MILLIS);
						OnSendRetrying();
					}
				}
			}
			break;
		case SendStatusEnum::SentOk:
			OnSendOk(Millis() - SendStartMillis);
#ifdef DEBUG_PACKET_SERVICE
			Serial.print(F("OnSendOk:"));
			if (Packet != nullptr && Packet->GetDefinition() != nullptr)
				Serial.println(Packet->GetDefinition()->GetHeader());
			else
				Serial.println(0);
#endif
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
#ifdef DEBUG_PACKET_SERVICE
			Serial.print(F("SendFailed:"));
			if (Packet != nullptr && Packet->GetDefinition() != nullptr)
				Serial.println(Packet->GetDefinition()->GetHeader());
			else
				Serial.println(0);
#endif
			ClearSendRequest();
			break;
		case SendStatusEnum::WaitingForAck:
			SendStatus = SendStatusEnum::AckFailed;
			SetNextRunASAP();
			break;
		case SendStatusEnum::AckOk:
			OnAckReceived(Packet->GetDefinition()->GetHeader(), Packet->GetId());
#ifdef DEBUG_PACKET_SERVICE
			Serial.print(F("OnAckReceived:"));
			if (Packet != nullptr && Packet->GetDefinition() != nullptr)
				Serial.println(Packet->GetDefinition()->GetHeader());
			else
				Serial.println(0);
#endif
			ClearSendRequest();
			break;
		case SendStatusEnum::AckFailed:
			OnAckFailed(Packet->GetDefinition()->GetHeader(), Packet->GetId());
#ifdef DEBUG_PACKET_SERVICE
			Serial.print(F("OnAckFailed:"));
			if (Packet != nullptr && Packet->GetDefinition() != nullptr)
				Serial.println(Packet->GetDefinition()->GetHeader());
			else
				Serial.println(0);
#endif
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
			SendStartMillis = Millis();
			SendTimeOutDuration = sendTimeOutDurationMillis;
			AckTimeOutDuration = ackReplyTimeOutDurationMillis;
			OverrideSendPermission = overrideSendPermission;
			SendStatus = SendStatusEnum::Requested;
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
		if (HasSendPendingInternal() && Packet->GetDefinition()->GetHeader() == header && Packet->GetId() == id)
		{
			if (SendStatus == SendStatusEnum::WaitingForAck)
			{
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