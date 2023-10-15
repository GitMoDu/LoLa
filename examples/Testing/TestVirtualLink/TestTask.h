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
	static const uint8_t PayloadSize = 0;
	static const uint32_t HearBeatPeriod = 1000;
	static const uint32_t PingPeriod = 321;

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
			Serial.println(F("# Link Clock"));
			Serial.print(F("Server: "));
			PrintDuration(ServerStatus.DurationSeconds);
			ServerStatus.Log(Serial);
			Serial.print(F("Client: "));
			PrintDuration(ClientStatus.DurationSeconds);
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
				Serial.println(F("Sending."));
#endif
				if (Server->SendPacket(OutData.Data, LoLaPacketDefinition::GetDataSizeFromPayloadSize(PayloadSize)))
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
		const uint32_t timestamp = micros();
		PrintTag('C');
		//Serial.print(port);
		//Serial.print(payloadSize);


		Serial.print(F("Received ("));
		Serial.print(startTimestamp);
		Serial.print(')');
		//Serial.print('|');
		//Serial.print(payloadSize);
		//Serial.print('|');


		//Serial.print('|');
		//if (payloadSize > 0)
		//{
		//	for (uint8_t i = 0; i < payloadSize; i++)
		//	{
		//		Serial.print(payload[0]);
		//		Serial.print('|');
		//	}
		//}
		//else
		//{
		//	Serial.print('|');
		//}
		Serial.println();
#endif
	}

	void PrintDuration(const uint32_t durationSeconds)
	{
		const uint32_t durationMinutes = durationSeconds / 60;
		const uint32_t durationHours = durationMinutes / 60;
		const uint32_t durationDays = durationHours / 24;
		const uint32_t durationYears = durationDays / 365;

		const uint32_t durationDaysRemainder = durationDays % 365;
		const uint32_t durationHoursRemainder = durationHours % 24;
		const uint32_t durationMinutesRemainder = durationMinutes % 60;
		const uint32_t durationSecondsRemainder = durationSeconds % 60;

		if (durationYears > 0)
		{
			Serial.print(durationYears);
			Serial.print(F(" years "));

			Serial.print(durationDaysRemainder);
			Serial.print(F(" days "));
		}
		else if (durationDays > 0)
		{
			Serial.print(durationDays);
			Serial.print(F(" days "));
		}

		if (durationHoursRemainder < 10)
		{
			Serial.print(0);
		}
		Serial.print(durationHoursRemainder);
		Serial.print(':');
		if (durationMinutesRemainder < 10)
		{
			Serial.print(0);
		}
		Serial.print(durationMinutesRemainder);
		Serial.print(':');
		if (durationSecondsRemainder < 10)
		{
			Serial.print(0);
		}
		Serial.print(durationSecondsRemainder);
		Serial.println();
	}
};
#endif
