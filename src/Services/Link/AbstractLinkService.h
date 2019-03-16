// AbstractLinkService.h

#ifndef _LOLA_ABSTRACT_LINK_SERVICE_h
#define _LOLA_ABSTRACT_LINK_SERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>

#include <LoLaCrypto\TinyCRC.h>
#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
#include <Services\LoLaDiagnosticsService\LoLaDiagnosticsService.h>
#endif

class AbstractLinkService : public IPacketSendService
{
private:
	PingPacketDefinition				DefinitionPing;
	LinkReportPacketDefinition			DefinitionReport;


	LinkShortPacketDefinition			DefinitionShort;
	LinkShortWithAckPacketDefinition	DefinitionShortWithAck;
	LinkLongPacketDefinition			DefinitionLong;

	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;

protected:
	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
		int32_t iint;
	};

	ArrayToUint32 ATUI_R;
	ArrayToUint32 ATUI_S;

	//Send packet.
	TemplateLoLaPacket<LOLA_LINK_SERVICE_PACKET_MAX_SIZE> OutPacket;

	LoLaServicesManager * ServicesManager = nullptr;

	LoLaLinkInfo* LinkInfo = nullptr;

	volatile uint32_t LastSent = ILOLA_INVALID_MILLIS;


public:
	AbstractLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, loLa, &OutPacket)
	{
	}

	bool SetServicesManager(LoLaServicesManager * servicesManager)
	{
		ServicesManager = servicesManager;

		if (ServicesManager != nullptr)
		{
			return OnAddSubServices();
		}
		return false;
	}

protected:
	virtual bool OnAddSubServices() { return true; }

	///Common packet handling.
	virtual void OnLinkAckReceived(const uint8_t header, const uint8_t id) {}
	virtual void OnPingAckReceived(const uint8_t id) {}

	//Unlinked packets.
	virtual bool OnAckedPacketReceived(ILoLaPacket* receivedPacket) { return false; }

protected:
	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&DefinitionPing) ||
			!packetMap->AddMapping(&DefinitionReport) ||
			!packetMap->AddMapping(&DefinitionShort) ||
			!packetMap->AddMapping(&DefinitionShortWithAck) ||
			!packetMap->AddMapping(&DefinitionLong))
		{
			return false;
		}

		//Make sure our re-usable packet has enough space for all our packets.
		if (Packet->GetMaxSize() < DefinitionPing.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionReport.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShort.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShortWithAck.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionLong.GetTotalSize())
		{
			return false;
		}

		return true;
	}

	bool ProcessAckedPacket(ILoLaPacket* receivedPacket)
	{
		if (receivedPacket->GetDataHeader() == DefinitionPing.GetHeader())
		{
			switch (LinkInfo->GetLinkState())
			{
			case LoLaLinkInfo::LinkStateEnum::Linking:
				return true;
			case LoLaLinkInfo::LinkStateEnum::Linked:
				return true;
			default:
				return false;
			}
		}

		return OnAckedPacketReceived(receivedPacket);
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		if (header == DefinitionPing.GetHeader())
		{
			OnPingAckReceived(id);
		}
		else
		{
			OnLinkAckReceived(header, id);
		}
	}

	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(LOLA_LINK_SEND_FAIL_RETRY_PERIOD);
	}

	void ResetStateStartTime()
	{
		StateStartTime = millis();
	}

	uint32_t GetElapsedLastValidReceived()
	{
		return millis() - GetLoLa()->GetLastValidReceivedMillis();
	}

	uint32_t GetElapsedLastSent()
	{
		return millis() - GetLoLa()->GetLastValidSentMillis();
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime == ILOLA_INVALID_MILLIS)
		{
			return ILOLA_INVALID_MILLIS;
		}
		else
		{
			return millis() - StateStartTime;
		}
	}

	uint32_t GetElapsedSinceLastSent()
	{
		if (LastSent == ILOLA_INVALID_MILLIS)
		{
			return LastSent;
		}
		else
		{
			return millis() - LastSent;
		}
	}

	void ResetLastSentTimeStamp()
	{
		LastSent = ILOLA_INVALID_MILLIS;
	}

	void TimeStampLastSent()
	{
		LastSent = millis();
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link service"));
	}
#endif // DEBUG_LOLA

	bool ShouldProcessReceived()
	{
		return IsSetupOk();
	}



	///Packet builders.
	void PreparePing()
	{
		OutPacket.SetDefinition(&DefinitionPing);
		OutPacket.SetId(random(0, UINT8_MAX));
	}


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