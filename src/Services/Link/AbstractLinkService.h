// AbstractLinkService.h

#ifndef _LOLA_ABSTRACT_LINK_SERVICE_h
#define _LOLA_ABSTRACT_LINK_SERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <LoLaLinkInfo.h>

#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


class AbstractLinkService : public IPacketSendService
{
private:

	//Runtime helpers.
	uint32_t StateStartTime = 0;

	PacketDefinition::IPacketListener* LastNotifiedListener = nullptr;
	PacketDefinition::IPacketListener* NotifyListener = nullptr;

protected:
	LinkReportPacketDefinition			DefinitionReport;

	LinkShortPacketDefinition			DefinitionShort;
	LinkShortWithAckPacketDefinition	DefinitionShortWithAck;
	LinkLongPacketDefinition			DefinitionLong;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
		int32_t iint;
	};

	//Runtime helpers.
	ArrayToUint32 ATUI_R;
	ArrayToUint32 ATUI_S;

	//Send packet.
	TemplateLoLaPacket<LOLA_LINK_SERVICE_PACKET_MAX_SIZE> OutPacket;

	LoLaLinkInfo* LinkInfo = nullptr;

	volatile uint32_t LastSentMillis = 0;


public:
	AbstractLinkService(Scheduler* scheduler, ILoLaDriver* driver)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, driver, &OutPacket)
		, DefinitionReport(this)
		, DefinitionShort(this)
		, DefinitionShortWithAck(this)
		, DefinitionLong(this)
	{
	}

	virtual bool Setup()
	{
		if (!LoLaDriver->GetPacketMap()->AddMapping(&DefinitionReport) ||
			!LoLaDriver->GetPacketMap()->AddMapping(&DefinitionShort) ||
			!LoLaDriver->GetPacketMap()->AddMapping(&DefinitionShortWithAck) ||
			!LoLaDriver->GetPacketMap()->AddMapping(&DefinitionLong))
		{
			return false;
		}

		//Make sure our re-usable packet has enough space for all our packets.
		if (Packet->GetMaxSize() < DefinitionReport.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShort.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShortWithAck.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionLong.GetTotalSize())
		{
			return false;
		}

		return IPacketSendService::Setup();
	}


protected:
	///Common packet handling.
	virtual void OnLinkAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

public:
	virtual void OnAckOk(const uint8_t header, const uint8_t id, const uint32_t timestamp)
	{
		OnLinkAckReceived(header, id, timestamp);
	}

	//Overload and do nothing, since we are the emiter.
	virtual void OnLinkStatusChanged() {}

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
		uint8_t Count = LoLaDriver->GetPacketMap()->GetCount();

		LastNotifiedListener = nullptr;
		for (uint8_t i = 0; i < Count; i++)
		{
			NotifyListener = LoLaDriver->GetPacketMap()->GetServiceAt(i);
			if (NotifyListener != nullptr)
			{
				NotifyListener->OnLinkStatusChanged();
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

	//Link service should receive packets even without link, of course.
	bool ShouldProcessReceived()
	{
		return true;
	}

	///Packet builders.
	inline void PrepareReportPacket(const uint8_t subHeader)
	{
		OutPacket.SetDefinition(&DefinitionReport);
		OutPacket.SetId(subHeader);
	}

	inline void PrepareShortPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		OutPacket.SetDefinition(&DefinitionShort);
		OutPacket.SetId(requestId);
		OutPacket.GetPayload()[0] = subHeader;
	}

	inline void PrepareLongPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		OutPacket.SetDefinition(&DefinitionLong);
		OutPacket.SetId(requestId);
		OutPacket.GetPayload()[0] = subHeader;
	}

	inline void PrepareShortPacketWithAck(const uint8_t requestId)
	{
		OutPacket.SetDefinition(&DefinitionShortWithAck);
		OutPacket.SetId(requestId);
	}

	inline void S_ArrayToPayload()
	{
		OutPacket.GetPayload()[1] = ATUI_S.array[0];
		OutPacket.GetPayload()[2] = ATUI_S.array[1];
		OutPacket.GetPayload()[3] = ATUI_S.array[2];
		OutPacket.GetPayload()[4] = ATUI_S.array[3];
	}

	inline void ArrayToR_Array(uint8_t* incomingPayload)
	{
		ATUI_R.array[0] = incomingPayload[0];
		ATUI_R.array[1] = incomingPayload[1];
		ATUI_R.array[2] = incomingPayload[2];
		ATUI_R.array[3] = incomingPayload[3];
	}
};

#endif