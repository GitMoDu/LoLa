// TestTask.h

#ifndef _TESTTASK_h
#define _TESTTASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <ILoLaInclude.h>


/// <summary>
/// Raw Link usage test task, no inheritance from template service.
/// </summary>
class TestTask : private TS::Task
	, public virtual ILinkListener
{
private:
	static constexpr uint8_t Port = 0x42;
	static constexpr uint8_t PayloadSize = 1;
	static constexpr uint32_t HearBeatPeriod = 1000;
	static constexpr uint32_t PingPeriod = 1234;

private:
	uint32_t RxCount = 0;
	uint32_t TxCount = 0;

	uint32_t LastRan = 0;

	uint16_t LastTx = 0;
	uint16_t LastRx = 0;

	TemplateLoLaOutDataPacket<PayloadSize> OutData{};
	LoLaLinkExtendedStatus ServerStatus{};


	ILoLaLink* Server;
	ILoLaLink* Client;
	Print& Log;

public:
	TestTask(TS::Scheduler& scheduler, Print& log,
		ILoLaLink* server, ILoLaLink* client)
		: ILinkListener()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, Server(server)
		, Client(client)
		, Log(log)
	{
	}

	const bool Setup()
	{
		if (Client != nullptr)
		{
			if (Server != nullptr)
			{
				return Server->RegisterLinkListener(this);
			}
		}

		return false;
	}

	virtual bool Callback() final
	{
		TS::Task::delay(0);
		Server->GetLinkStatus(ServerStatus);
		ServerStatus.OnUpdate();

#if defined(SERVER_DROP_LINK_TEST)
		if (ServerStatus.DurationSeconds > SERVER_DROP_LINK_TEST)
		{
			Log.println(F("# Server Stop Link."));
			Server->Stop();
			Server->Start();
			Log.println(F("# Server Restart Link."));
		}
#endif

#if defined(CLIENT_DROP_LINK_TEST) 
		if (Client->HasLink() && ServerStatus.DurationSeconds > CLIENT_DROP_LINK_TEST)
		{
			Log.println(F("# Client Stop Link."));
			Client->Stop();
			Client->Start();
			Log.println(F("# Client Restart Link."));
		}
#endif

#if defined(PRINT_LINK_HEARBEAT)
		const uint32_t timestamp = millis();
		if (timestamp - LastRan >= HearBeatPeriod * PRINT_LINK_HEARBEAT)
		{
			LastRan = timestamp;

#if defined(DEBUG_LOLA)
			Log.print(F("Server: "));
			ServerStatus.LogLong(Log);
#endif
		}
#endif
		return true;
	}

private:
	void PrintTag(const char tag)
	{
		Log.print('[');
		Log.print(tag);
		Log.print(']');
		Log.print(' ');
		Log.print(micros());
		Log.print('-');
	}

public:
	virtual void OnLinkStateUpdated(const bool hasLink) final
	{
		if (hasLink)
		{
			Server->GetLinkStatus(ServerStatus);
			ServerStatus.OnStart();

#ifdef DEBUG_LOLA
			Log.print(millis());
			Log.println(F("\tLink Acquired."));
#endif
			TS::Task::enableDelayed(1000);
		}
		else
		{
#ifdef DEBUG_LOLA
			Log.print(millis());
			Log.println(F("\tLink Lost."));
#endif
			TS::Task::disable();
		}
	}
};
#endif