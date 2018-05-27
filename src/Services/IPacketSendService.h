// IPacketSendService.h

#ifndef _IPACKETSENDSERVICE_h
#define _IPACKETSENDSERVICE_h


#include <Arduino.h>
#include <Services\ILoLaService.h>
#include <Packet\LoLaPacket.h>

//Takes around the same time as a full time out.
#define LOLA_SEND_SERVICE_DENIED_BACK_OFF_DURATION_MILLIS 1
#define LOLA_SEND_SERVICE_DENIED_MAX_FAILS 3

#define LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS 30

#define LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS 1000

class IPacketSendService : public ILoLaService
{
public:
	IPacketSendService(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ILoLaPacket* packetHolder)
		: ILoLaService(scheduler, period, loLa)
	{
		Packet = packetHolder;
		Packet->ClearDefinition();
	}

	virtual bool OnEnable()
	{
		return true;
	}

	virtual void OnDisable()
	{
	}


	bool Callback()
	{
		//Finish sending any pending packet.
		switch (SendStatus)
		{
		case SendStatusEnum::Requested:
			if (HasSendPending())
			{
				SendFailures = 0;
				SendStatus = SendStatusEnum::SendingPacket;
			}
			else
			{
				SendStatus = SendStatusEnum::Done;
				SetNextRunASAP();
			}
			break;
		case SendStatusEnum::SendingPacket:

			if (Millis() > TimeOutPoint)
			{
				SetNextRunASAP();
				OnSendTimedOut();
				SendStatus = SendStatusEnum::Done;
			}
			else if (!HasSendPending())
			{
				SetNextRunASAP();
				SendStatus = SendStatusEnum::Done;
			}
			else if (!AllowedSend())
			{
				//Give an opportunity for the service to update the packet, if needed.
				SetNextRunDelay(LOLA_SEND_SERVICE_DENIED_BACK_OFF_DURATION_MILLIS);
				OnSendDelayed();
			}
			else
			{
				//We only fire the OnSend and OnSendFailed events in the next cycle.
				//Because SendPacket() is already quite time consuming.
				if (SendPacket(Packet))
				{
					SendStatus = SendStatusEnum::Sent;
					SetNextRunASAP();
				}
				else
				{
					SendFailures++;
					if (SendFailures > LOLA_SEND_SERVICE_DENIED_MAX_FAILS)
					{
						ClearSendRequest();
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
		case SendStatusEnum::Sent:
			OnSendOk();
			SendStatus = SendStatusEnum::Done;
			break;
		case SendStatusEnum::SendFailed:
			OnSendFailed();
			SendStatus = SendStatusEnum::Done;
			break;
		case SendStatusEnum::Done:
		default:
			SetNextRunDefault();
			OnService();
			break;
		}

		return true;
	}

private:
	enum SendStatusEnum
	{
		Requested,
		SendingPacket,
		Sent,
		SendFailed,
		Done
	} SendStatus = SendStatusEnum::Done;

	uint8_t SendFailures = 0;

protected:
	uint32_t TimeOutPoint = 0;
	ILoLaPacket * Packet = nullptr;

protected:
	virtual void OnSendOk() { }
	virtual void OnSendFailed() { SetNextRunDelay(LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS); }
	virtual void OnService() { SetNextRunDelay(LOLA_SEND_SERVICE_BACK_OFF_DEFAULT_DURATION_MILLIS); }
	virtual void OnSendTimedOut() { }
	virtual void OnSendDelayed() { }
	virtual void OnSendRetrying() { }
	virtual bool OnSetup()
	{
		//TODO: Get comms entropy source, abstracted.
		randomSeed(analogRead(0));
		return Packet != nullptr;
	}
protected:
	void RequestSendPacket(const uint8_t timeOutDurationMillis = LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS)
	{
		TimeOutPoint = Millis() + timeOutDurationMillis;
		SendStatus = SendStatusEnum::Requested;
		SetNextRunASAP();
	}

	bool HasSendPending()
	{
		return Packet->GetDefinition() != nullptr;
	}

	void ClearSendRequest()
	{
		Packet->ClearDefinition();
	}
};

#endif

