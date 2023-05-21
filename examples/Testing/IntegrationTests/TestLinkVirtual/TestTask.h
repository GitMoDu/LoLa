// TestTask.h

#ifndef _TESTTASK_h
#define _TESTTASK_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>


class TestTask : private Task
	, public virtual ILinkPacketSender
	, public virtual ILinkPacketReceiver
	, public virtual ILinkListener
{
private:
	uint32_t LastRan = 0;
	uint32_t LastPing = 0;
	static const uint8_t Port = 0x42;
	static const uint8_t PayloadSize = 0;
	static const uint32_t HearBeatPeriod = 1000;
	static const uint32_t PingPeriod = 321;

	ILoLaLink* Host;
	ILoLaLink* Remote;

	TemplateLoLaOutDataPacket<PayloadSize> OutData;


public:
	TestTask(Scheduler& scheduler, ILoLaLink* host, ILoLaLink* remote)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILinkPacketSender()
		, ILinkPacketReceiver()
		, Host(host)
		, Remote(remote)
	{
		LastPing = millis();
	}

	const bool Setup()
	{
		if (Remote != nullptr)
		{
			if (Remote->RegisterPacketReceiver(this, Port))
			{
				if (Host != nullptr)
				{
					return Host->RegisterLinkListener(this);
				}
			}
		}


		return false;
	}

	uint8_t Payload = 0;
	virtual bool Callback() final
	{
		bool workDone = false;
		uint32_t timestamp = millis();

		// Heartbeat
#if defined(PRINT_LINK_HEARBEAT)
		if (timestamp - LastRan >= HearBeatPeriod * PRINT_LINK_HEARBEAT)
		{
			workDone = true;
			LastRan = timestamp;
			Serial.println(F("# Link Clock"));
			Serial.print(F("Host:"));
			PrintDuration(Host->GetLinkDuration());
			//Serial.println(Host->GetLinkDuration());
			Serial.print(F("Remote: "));
			//Serial.println(Remote->GetLinkDuration());
			PrintDuration(Remote->GetLinkDuration());

			Serial.println(F("# DebugClock"));
			Host->DebugClock();
			Remote->DebugClock();
			Serial.println();
		}
#endif

		if ((Remote != nullptr) && ((timestamp - LastPing) >= PingPeriod))
		{
			workDone = true;
			OutData.SetPort(Port);

			if (Host->CanSendPacket(PayloadSize))
			{
				Payload++;
				LastPing = timestamp;
#if defined(PRINT_TEST_PACKETS)
				PrintTag('H');
				Serial.println(F("Sending."));
#endif
				if (Host->SendPacket(this, OutData.Data, LoLaPacketDefinition::GetDataSizeFromPayloadSize(PayloadSize)))
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
	virtual void OnSendComplete(const SendResultEnum result) final
	{
#if defined(PRINT_TEST_PACKETS)
		PrintTag('H');

		switch (Success)
		{
		case SendResultEnum::Success:
			Serial.println(F("Sent Success"));
			break;
		case SendResultEnum::SendTimeout:
			Serial.println(F("Send Timeout"));
			break;
		case SendResultEnum::SendCollision:
			Serial.println(F(" Send Collision"));
			break;
		case SendResultEnum::Error:
			Serial.println(F("Send Error"));
			break;
		default:
			break;
		}
#endif
	}

public:
	virtual void OnLinkStateUpdated(const bool hasLink)
	{
		Serial.print(millis());
		if (hasLink)
		{
			Serial.println(F("\tLink Acquired."));
			Task::enable();
		}
		else
		{
			Serial.println(F("\tLink Lost."));
			Task::disable();
		}
	}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		const uint32_t timestamp = micros();
#if defined(PRINT_TEST_PACKETS)
		PrintTag('R');
		//Serial.print(port);
		//Serial.print(payloadSize);


		Serial.print(F("Received ("));
		//Serial.print(startTimestamp);
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
