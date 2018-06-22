// LoLaServicesManager.h

#ifndef _RADIOSERVICESMANAGER_h
#define _RADIOSERVICESMANAGER_h


#include <Packet\LoLaPacket.h>
#include <Services\ILoLaService.h>

#ifndef LOLA_SERVICES_MAX_COUNT
#define MAX_RADIO_SERVICES_COUNT 10
#endif

class LoLaServicesManager
{
private:
	ILoLaService * Services[MAX_RADIO_SERVICES_COUNT];
	uint8_t ServicesCount = 0;
	bool Error = false;

public:
	LoLaServicesManager()
	{
		for (uint8_t i = 0; i < MAX_RADIO_SERVICES_COUNT; i++)
		{
			Services[i] = nullptr;
		}
	}

	void ProcessPacket(ILoLaPacket* receivedPacket)
	{
		uint8_t header = receivedPacket->GetDataHeader();
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr && Services[i]->ProcessPacket(receivedPacket, header))
			{
				return;
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
		uint8_t header = receivedPacket->GetPayload()[0];
		uint8_t id = receivedPacket->GetPayload()[1];

		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr && Services[i]->ProcessAck(header, id))
			{
				return;
			}
		}
	}

	ILoLaService* Get(uint8_t index)
	{
		if (index > ServicesCount || index > MAX_RADIO_SERVICES_COUNT)
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

	bool StartAll()
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr)
			{
				Services[i]->Enable();
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	bool StopAll()
	{
		for (uint8_t i = 0; i < ServicesCount; i++)
		{
			if (Services[i] != nullptr)
			{
				Services[i]->Disable();
			}
			else
			{
				return false;
			}
		}

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
		//Benchmark services check.
		uint32_t Start, End;
		Start = micros();
		for (uint8_t i = 0; i < MAX_RADIO_SERVICES_COUNT; i++)
		{
			if (Services[i] != nullptr)
			{
				Services[i]->Test(i);
			}
			else
			{
				Services[0]->Test(i);
			}
		}
		End = micros();

		serial->print(F("Services Look Up worst case benchmark took: "));
		serial->print(End - Start);
		serial->print(F(" us."));
		serial->println();

	}
#endif
};

#endif

