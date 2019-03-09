// LoLaServicesManager.h

#ifndef _RADIOSERVICESMANAGER_h
#define _RADIOSERVICESMANAGER_h

//#define DEBUG_PACKET_INPUT

#include <Packet\LoLaPacket.h>
#include <Services\ILoLaService.h>

#include <LoLaLinkInfo.h>

#ifndef LOLA_SERVICES_MAX_COUNT
#define MAX_RADIO_SERVICES_COUNT 10
#endif

class LoLaServicesManager
{
private:
	ILoLaService * Services[MAX_RADIO_SERVICES_COUNT];
	uint8_t ServicesCount = 0;
	bool Error = false;


	///Link Info Source
	LoLaLinkInfo LinkInfo;
	///

public:
	LoLaServicesManager() : LinkInfo()
	{
		for (uint8_t i = 0; i < MAX_RADIO_SERVICES_COUNT; i++)
		{
			Services[i] = nullptr;
		}
	}
	
	void ProcessPacket(ILoLaPacket* receivedPacket)
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr && Services[i]->ReceivedPacket(receivedPacket))
			{
				return;
			}
		}
	}

	bool ProcessAckedPacket(ILoLaPacket* receivedPacket)
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr && Services[i]->ReceivedAckedPacket(receivedPacket))
			{
				return true;
			}
		}

		return false;
	}

	void ProcessSent(const uint8_t header)
	{
		if (header != PACKET_DEFINITION_ACK_HEADER)
		{
			for (uint8_t i = 0; i < ServicesCount; i++)
			{
				if (Services[i] != nullptr && Services[i]->ProcessSent(header))
				{
					return;
				}
			}
		}
	}

	void NotifyServicesLinkUpdated(const bool connected)
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr)
			{
				if (connected)
				{
					Services[i]->OnLinkEstablished();
				}
				else
				{
					Services[i]->OnLinkLost();
				}
			}
		}
	}

	void ProcessAck(ILoLaPacket* receivedPacket)
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr && Services[i]->ProcessAck(receivedPacket->GetPayload()[0], receivedPacket->GetId()))
			{
				return;
			}
		}		
	}

	ILoLaService* Get(uint8_t index)
	{
		if (index >= ServicesCount || index >= MAX_RADIO_SERVICES_COUNT)
		{
			return nullptr;
		}
		else
		{
			return Services[index];
		}
	}

	uint8_t GetCount()
	{
		return ServicesCount;
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
	}

	bool Add(ILoLaService* service)
	{
		if (ServicesCount >= MAX_RADIO_SERVICES_COUNT ||
			service == nullptr)
		{
			return false;
		}

		if (!service->Init())
		{
			return false;
		}

		Services[ServicesCount] = service;
		ServicesCount++;

		return true;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(F("LoLa Services: "));
		serial->println(GetCount());

		if (GetCount() == 0)
		{
			return;
		}
		for (uint8_t i = 0; i < GetCount(); i++)
		{
			if (Services[i] != nullptr)
			{
				Services[i]->Debug(serial);
				serial->println();
			}
		}
	}
#endif
};
#endif