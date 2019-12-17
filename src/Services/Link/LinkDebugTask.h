// LinkDebugTask.h

#ifndef _LINKDEBUGTASK_h
#define _LINKDEBUGTASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <LoLaDefinitions.h>


class LinkDebugTask : Task
{
private:
	uint32_t NextDebug = 0;

	uint32_t PKCDuration = 0;
	uint32_t LinkingDuration = 0;

	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

	ILoLaClockSource* SyncedClock = nullptr;
	LoLaCryptoKeyExchanger* KeyExchanger = nullptr;

	const static uint32_t UPDATE_PERIOD = LOLA_LINK_DEBUG_UPDATE_SECONDS * 1000;

public:
	LinkDebugTask(Scheduler* scheduler,
		ILoLaDriver* loLaDriver,
		LoLaLinkInfo* linkInfo,
		ILoLaClockSource* syncedClock,
		LoLaCryptoKeyExchanger* keyExchanger)
		: Task(UPDATE_PERIOD, TASK_FOREVER, scheduler, false)
	{
		LoLaDriver = loLaDriver;
		LinkInfo = linkInfo;
		SyncedClock = syncedClock;
		KeyExchanger = keyExchanger;
	}

	void Debug()
	{
		Serial.print(F("Local MAC: "));
		LinkInfo->PrintMac(&Serial);
		Serial.println();
		Serial.print(F("\tId: "));
		Serial.println(LinkInfo->GetLocalId());

#ifdef LOLA_LINK_USE_ENCRYPTION
		Serial.println(F("Link secured with"));
		Serial.print(F("160 bit "));
		KeyExchanger->Debug(&Serial);
		Serial.println();
		Serial.print(F("\tEncrypted with 128 bit cypher "));
		LoLaDriver->GetCryptoEncoder()->Debug(&Serial);
		Serial.println();
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		Serial.print(F("\tChannel hopping"));
		Serial.print(LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS);
		Serial.println(F(" ms"));
#endif
#ifdef LOLA_LINK_USE_TOKEN_HOP
		Serial.print(F("\tProtected with 32 bit TOTP @ "));
		Serial.print(LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS);
		Serial.println(F(" ms"));
#endif
#else
		Serial.println(F("Link unsecured."));
#endif

#ifdef LOLA_LINK_ACTIVITY_LED
		pinMode(LOLA_LINK_ACTIVITY_LED, OUTPUT);
#endif
	}

	void OnLinkOn()
	{
		LinkingDuration = millis() - LinkingDuration;
		DebugLinkEstablished();
		enable();
	}

	void OnLinkOff()
	{
		disable();
		DebugLinkStatistics(&Serial);
	}

	void OnPKCStarted()
	{
		PKCDuration = millis();
	}

	void OnPKCDone()
	{
		PKCDuration = millis() - PKCDuration;
	}

	void OnLinkingStarted()
	{
		LinkingDuration = millis();
		Serial.print(F("Linking to Id: "));
		Serial.print(LinkInfo->GetPartnerId());
		Serial.print(F("\tSession: "));
		Serial.println(LinkInfo->GetSessionId());
		Serial.print(F("PKC took "));
		Serial.print(PKCDuration);
		Serial.println(F(" ms."));
	}

	bool OnEnable()
	{
		return LinkInfo != nullptr && LoLaDriver != nullptr;
	}

	bool Callback()
	{
		DebugLinkStatistics(&Serial);

		return true;
	}

private:
	void DebugLinkEstablished()
	{
		Serial.print(F("Linked: "));
		Serial.println(SyncedClock->GetSyncSeconds());
		Serial.print(F(" @ SyncMicros: "));
		Serial.println(SyncedClock->GetSyncMicros());
		Serial.print(F("Linking took "));
		Serial.print(LinkingDuration);
		Serial.println(F(" ms."));
		Serial.print(F("Round Trip Time: "));
		Serial.print(LinkInfo->GetRTT());
		Serial.println(F(" us"));
		Serial.print(F("Latency compensation: "));
		Serial.print(LoLaDriver->GetETTMMicros());
		Serial.println(F(" us"));
	}

	void DebugLinkStatistics(Stream* serial)
	{
		serial->println();
		serial->println(F("Link Info"));

		uint32_t AliveSeconds = LinkInfo->GetLinkDurationSeconds();

		if (AliveSeconds / 86400 > 0)
		{
			serial->print(AliveSeconds / 86400);
			serial->print(F("d "));
			AliveSeconds = AliveSeconds % 86400;
		}

		if (AliveSeconds / 3600 < 10)
			serial->print('0');
		serial->print(AliveSeconds / 3600);
		serial->print(':');
		if ((AliveSeconds % 3600) / 60 < 10)
			serial->print('0');
		serial->print((AliveSeconds % 3600) / 60);
		serial->print(':');
		if ((AliveSeconds % 3600) % 60 < 10)
			serial->print('0');
		serial->println((AliveSeconds % 3600) % 60);

		/*serial->print(F("ClockSync: "));
		serial->println(LoLaDriver->GetClockSource()->GetSyncSeconds());
		serial->print(F(" @ Offset: "));
		serial->println(LoLaDriver->GetClockSource()->GetSyncMicros());*/


		serial->print(F("Transmit Power: "));
		serial->print((float)(((LinkInfo->GetTransmitPowerNormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));

		serial->print(F("RSSI: "));
		serial->print((float)(((LinkInfo->GetRSSINormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));
		serial->print(F("RSSI Partner: "));
		serial->print((float)(((LinkInfo->GetPartnerRSSINormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));

		serial->print(F("Sent: "));
		serial->println(LoLaDriver->GetTransmitedCount());
		serial->print(F("Lost: "));
		serial->println(LinkInfo->GetLostCount());
		serial->print(F("Received: "));
		serial->println(LoLaDriver->GetReceivedCount());
		serial->print(F("Rejected: "));
		serial->print(LoLaDriver->GetRejectedCount());

		serial->print(F(" ("));
		if (LoLaDriver->GetReceivedCount() > 0)
		{
			serial->print((float)(LoLaDriver->GetRejectedCount() * 100) / (float)LoLaDriver->GetReceivedCount(), 2);
			serial->println(F(" %)"));
		}
#ifdef DEBUG_LOLA
		else if (LoLaDriver->GetRejectedCount() > 0)
		{
			serial->println(F("100 %)"));
		}
#endif
		else
		{
			serial->println(F("0 %)"));
		}
		serial->print(F("Timming Collision: "));
		serial->println(LoLaDriver->GetTimingCollisionCount());

		serial->print(F("ClockSync adjustments: "));
		serial->println(LinkInfo->GetClockSyncAdjustments());
		serial->println();
	}
};


#endif

