// AbstractLinkService.h

#ifndef _LOLA_ABSTRACT_LINK_SERVICE_h
#define _LOLA_ABSTRACT_LINK_SERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\Link\LoLaLinkDefinitions.h>
#include <Services\Link\ProtocolVersionCalculator.h>

#include <LoLaLinkInfo.h>

#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


class AbstractLinkService : public IPacketSendService
{
protected:
	union ArrayToUint32 {
		uint8_t array[4];
		uint32_t uint;
		int32_t iint;
	};

private:
	// Runtime helpers.
	uint32_t StateStartTime = 0;

	IPacketListener* LastNotifiedListener = nullptr;
	IPacketListener* NotifyListener = nullptr;


	//LinkShortPacketDefinition			DefinitionShort;



protected:
	LinkPKERequestPacketDefinition		DefinitionPKERequest;
	LinkPKEResponsePacketDefinition		DefinitionPKEResponse;
	LinkStartWithAckPacketDefinition	DefinitionStartLink;

	LinkMultiPacketDefinition			DefinitionMulti;
	LinkReportPacketDefinition			DefinitionReport;

	ProtocolVersionCalculator ProtocolVersion;

	//Send packet.
	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> OutPacket;

	LoLaLinkInfo* LinkInfo = nullptr;

	enum LinkStateEnum : uint8_t
	{
		Disabled = 0,
		StartLink = 1,
		AwaitingLink = 2,
		AwaitingSleeping = 3,
		Linking = 4,
		Linked = 5
	};

	LinkStateEnum LinkState = LinkStateEnum::Disabled;

	// Runtime helpers.
	ArrayToUint32 ATUI_R;
	uint32_t LastSentMillis = 0;
	ArrayToUint32 ATUI_S;

public:
	AbstractLinkService(Scheduler* scheduler, ILoLaDriver* driver)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, driver, &OutPacket)
		, DefinitionPKERequest(this)
		, DefinitionPKEResponse(this)
		, DefinitionStartLink(this)
		, DefinitionMulti(this)
		, DefinitionReport(this)
	{
	}

	virtual bool Setup()
	{
		//Make sure our re-usable packet has enough space.
		if (Packet->GetMaxSize() < DefinitionPKERequest.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionPKEResponse.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionStartLink.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionMulti.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionReport.GetTotalSize())
		{
			return false;
		}

		if (!LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionPKERequest) ||
			!LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionPKEResponse) ||
			!LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionStartLink) ||
			!LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionMulti) ||
			!LoLaDriver->GetPacketMap()->RegisterMapping(&DefinitionReport))
		{
			return false;
		}

		return IPacketSendService::Setup();
	}

public:
	//Overload and do nothing, since we are the emiter.
	virtual void OnLinkStatusChanged(const bool linked) {}


#ifdef DEBUG_LOLA
	uint8_t GetLinkState()
	{
		return LinkState;
	}
#endif // DEBUG_LOLA

protected:

	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(LOLA_LINK_SEND_FAIL_RETRY_PERIOD);
	}

	void ResetStateStartTime()
	{
		StateStartTime = millis();
	}

	uint32_t GetElapsedMillisSinceStateStart()
	{
		if (StateStartTime == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return millis() - StateStartTime;
		}
	}

	uint32_t GetElapsedMillisSinceLastSent()
	{
		if (LastSentMillis == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return millis() - LastSentMillis;
		}
	}

	void ResetLastSentTimeStamp()
	{
		LastSentMillis = 0;
	}

	void TimeStampLastSent()
	{
		LastSentMillis = millis();
	}

	void NotifyServicesLinkStatusChanged()
	{
		uint8_t Count = LoLaDriver->GetPacketMap()->GetMaxIndex();

		LastNotifiedListener = nullptr;

		// Skip first mapping, it's gonna be the Ack definition.
		for (uint8_t i = PACKET_DEFINITION_ACK_HEADER + 1; i < Count; i++)
		{
			NotifyListener = LoLaDriver->GetPacketMap()->GetServiceAt(i);
			if (NotifyListener != nullptr)
			{
				NotifyListener->OnLinkStatusChanged(LinkInfo->HasLink());
				LastNotifiedListener = NotifyListener;
			}
		}
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link service"));
	}
#endif // DEBUG_LOLA
};
#endif