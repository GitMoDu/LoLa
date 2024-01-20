// TestTask.h

#ifndef _TESTTASK_h
#define _TESTTASK_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>


/// <summary>
/// Raw Link usage test task, no inheritance from template service.
/// </summary>
class TestTask : private Task
	, public virtual ILinkPacketListener
	, public virtual ILinkListener
{
private:
	uint32_t LastRan = 0;
	uint32_t LastPing = 0;
	static const uint8_t Port = 0x42;
	static const uint8_t PayloadSize = 1;
	static const uint32_t HearBeatPeriod = 1000;
	static const uint32_t PingPeriod = 1234;

	ILoLaLink* Server;
	ILoLaLink* Client;

	TemplateLoLaOutDataPacket<PayloadSize> OutData;


public:
	TestTask(Scheduler& scheduler, ILoLaLink* host, ILoLaLink* remote)
		: ILinkPacketListener()
		, ILinkListener()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, Server(host)
		, Client(remote)
		, OutData()
	{
		LastPing = millis();
	}

	const bool Setup()
	{
		if (Client != nullptr)
		{
			if (Client->RegisterPacketListener(this, Port))
			{
				if (Server != nullptr)
				{
					return Server->RegisterLinkListener(this);
				}
			}
		}

		return false;
	}

	LoLaLinkStatus ServerStatus{};
	LoLaLinkStatus ClientStatus{};

	uint8_t Payload = 0;
	virtual bool Callback() final
	{
		bool workDone = false;
		const uint32_t timestamp = millis();

		// Update links' status.
		Server->GetLinkStatus(ServerStatus);
		Client->GetLinkStatus(ClientStatus);

#if defined(SERVER_DROP_LINK_TEST) 
		if (Server->HasLink() && ServerStatus.DurationSeconds > SERVER_DROP_LINK_TEST)
		{
			Serial.println(F("# Server Stop Link."));
			Server->Stop();
			Server->Start();
		}
#endif

#if defined(CLIENT_DROP_LINK_TEST) 
		if (Client->HasLink() && ClientStatus.DurationSeconds > CLIENT_DROP_LINK_TEST)
		{
			Serial.println(F("# Client Stop Link."));
			Client->Stop();
			Client->Start();
		}
#endif

#if defined(PRINT_LINK_HEARBEAT)
		if (timestamp - LastRan >= HearBeatPeriod * PRINT_LINK_HEARBEAT)
		{
			workDone = true;
			LastRan = timestamp;
			Serial.println();
			Serial.println(F("# Link Clock"));
			Serial.print(F("Server: "));
			ServerStatus.Log(Serial);
			Serial.print(F("Client: "));
			ClientStatus.Log(Serial);
		}
#endif

		if ((Client != nullptr) && ((timestamp - LastPing) >= PingPeriod))
		{
			workDone = true;
			OutData.SetPort(Port);

			if (Server->CanSendPacket(PayloadSize))
			{
				Payload++;
				LastPing = timestamp;
#if defined(PRINT_TEST_PACKETS)
				PrintTag('H');
				Serial.print(F("Sending "));
				if (PayloadSize > 0)
				{
					Serial.print('|');
					for (uint8_t i = 0; i < PayloadSize; i++)
					{
						Serial.print(OutData.Payload[i]);
						Serial.print('|');
					}
				}
				Serial.println();
#endif
				if (Server->SendPacket(OutData.Data, PayloadSize))
				{
					LastPing = timestamp;
				}
				else
				{
#if defined(PRINT_TEST_PACKETS)
					Serial.println(F(" Failed!"));
#endif
				}
			}
		}

		return workDone;
	}
private:
	void PrintTag(const char tag)
	{
		Serial.print('[');
		Serial.print(tag);
		Serial.print(']');
		Serial.print(' ');
		Serial.print(micros());
		Serial.print('-');
	}

public:
	virtual void OnSendRequestTimeout() final
	{
#if defined(DEBUG_LOLA)
		PrintTag('S');
		Serial.println(F("Send Error."));
#endif
	}

public:
	virtual void OnLinkStateUpdated(const bool hasLink)
	{
		if (hasLink)
		{
#ifdef DEBUG_LOLA
			Serial.print(millis());
			Serial.println(F("\tLink Acquired."));
#endif
			Task::enable();
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.print(millis());
			Serial.println(F("\tLink Lost."));
#endif
			Task::disable();
		}
	}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
#if defined(PRINT_TEST_PACKETS)
		PrintTag('C');
		Serial.print(F("Received "));

		if (payloadSize > 0)
		{
			Serial.print('|');
			for (uint8_t i = 0; i < payloadSize; i++)
			{
				Serial.print(payload[i]);
				Serial.print('|');
			}
		}
		Serial.println();
#endif
	}
};
#endif
