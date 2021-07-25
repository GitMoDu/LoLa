// LoLaPacketService.h

#ifndef _LOLA_PACKET_SERVICE_h
#define _LOLA_PACKET_SERVICE_h


#include <Arduino.h>
#include <Services\AbstractLoLaService.h>
#include <Packet\LoLaPacket.h>

template<const uint8_t OutPacketPayloadSize,
	const bool LinkCheck = true>
	class LoLaPacketService : public AbstractLoLaService
{
private:
	static const uint32_t SEND_CHECK_PERIOD_MILLIS = 1;

protected:
	union ArrayToUint32 {
		uint8_t array[4];
		uint32_t uint;
		int32_t iint;
	};

private:
	enum SendStatusEnum
	{
		Done = 0,
		SendingPacket = 1,
		SendOk = 2,
		WaitingForAck = 3
	} SendStatus = SendStatusEnum::Done;

	static const uint32_t SendTimeOutDuration = (3 * ILOLA_DUPLEX_PERIOD_MILLIS);
	static const uint32_t ReplyTimeOutDuration = (ILOLA_DUPLEX_PERIOD_MILLIS / 4);


	uint32_t SendTimeOutTimestamp = 0;

	struct LastSentInfo
	{
		uint32_t Millis = 0;
		uint8_t Id = 0;
	} LastSent;

	PacketDefinition* PendingDefinition = nullptr;


private:
	// Outgoing packet message.
	uint8_t OutMessage[OutPacketPayloadSize];

protected:
	virtual void OnService()
	{
		SetNextRunDelay(UINT32_MAX);
	}

	virtual void OnSendTimedOut(const uint8_t header) {
		//Serial.println(F("OnSendTimedOut"));
	}
	virtual void OnAckFailed(const uint8_t header) { }
	virtual void OnAckOk(const uint8_t header) { }
	virtual void OnPreSend(const uint8_t header) { }
	virtual void OnSendOk(const uint8_t header) { }

public:
	LoLaPacketService(Scheduler* scheduler, const uint32_t period, LoLaPacketDriver* driver)
		: AbstractLoLaService(scheduler, period, driver)
	{
	}

protected:
	uint8_t* GetOutPayload()
	{
		return OutMessage;
	}

public:
	virtual void OnAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		if (SendStatus == SendStatusEnum::WaitingForAck &&
			PendingDefinition != nullptr &&
			PendingDefinition->Header == header &&
			LastSent.Id == id)
		{
			OnAckOk(header);
			ClearSendRequest();
		}
	}

	virtual bool Setup()
	{
		return AbstractLoLaService::Setup();
	}

	bool ValidateMessageSize(const uint8_t messageSize)
	{
		return messageSize <= OutPacketPayloadSize;
	}

	virtual bool Callback()
	{
		// Finish sending any pending packet.
		switch (SendStatus)
		{
		case SendStatusEnum::SendingPacket:
			if (LoLaDriver->AllowedSend())
			{
				// Last second, fast stuff.
				OnPreSend(PendingDefinition->Header);

				// SendPacket is quite time consuming and can fail.
				if (LoLaDriver->SendPacket(PendingDefinition, OutMessage, LastSent.Id))
				{
					LastSent.Millis = millis();
					SendStatus = SendStatusEnum::SendOk;
				}

				SetNextRunASAP();
			}
			else if (millis() > SendTimeOutTimestamp)
			{
				ClearSendRequest();
				OnSendTimedOut(PendingDefinition->Header);
			}
			else
			{
				//Serial.print(F("!AllowedSend"));
				//Serial.println(millis());
				SetNextRunDelay(SEND_CHECK_PERIOD_MILLIS);
			}
			break;
		case SendStatusEnum::SendOk:
			OnSendOk(PendingDefinition->Header);

			if (PendingDefinition->HasAck)
			{
				SendStatus = SendStatusEnum::WaitingForAck;
				SetNextRunDelay(ReplyTimeOutDuration);
			}
			else
			{
				ClearSendRequest();
			}
			break;
		case SendStatusEnum::WaitingForAck:
			OnAckFailed(PendingDefinition->Header);
			ClearSendRequest();
			break;
		case SendStatusEnum::Done:
			OnService();
			break;
		default:
			ClearSendRequest();
			break;
		}

		return true;
	}

protected:
	bool RequestSendPacket(const uint8_t header)
	{
		if (!LinkCheck || (LoLaDriver->LinkInfo.HasLink()))
		{
			PendingDefinition = LoLaDriver->PacketMap.GetDefinition(header);

			if (PendingDefinition != nullptr)
			{
				SendTimeOutTimestamp = millis() + SendTimeOutDuration;
				SendStatus = SendStatusEnum::SendingPacket;
				SetNextRunASAP();

				return true;
			}
		}

		return false;
	}

	const bool IsSending()
	{
		return SendStatus != SendStatusEnum::Done;
	}

	void ClearSendRequest()
	{
		SendStatus = SendStatusEnum::Done;
		SetNextRunASAP();
	}

	// Value in milliseconds.
	const uint32_t GetElapsedSinceLastSent()
	{
		if (LastSent.Millis != 0)
		{
			return millis() - LastSent.Millis;
		}
		else
		{
			return UINT32_MAX;
		}
	}

	void ResetLastSentTimeStamp()
	{
		//LastSent.Millis = 0;
	}
};
#endif