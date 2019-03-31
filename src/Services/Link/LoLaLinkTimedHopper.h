// LoLaLinkTimedHopper.h

#ifndef _LOLA_TIMED_HOPPER_h
#define _LOLA_TIMED_HOPPER_h

#include <Services\ILoLaService.h>
#include <LoLaClock\ILoLaClockSource.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <LoLaCrypto\LoLaCryptoTokenSource.h>
#include <Services\Link\LoLaLinkChannelManager.h>


class LoLaTimeDebugTask : public Task
{
private:
	//Synced clock.
	ILoLaClockSource* SyncedClock = nullptr;

	LoLaCryptoTokenSource*	CryptoSeed = nullptr;

	const uint32_t TestPeriodMillis = ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS;
	const uint32_t TestHalfPeriodMillis = TestPeriodMillis / 2;
	const uint8_t OutputPin = PB12;

public:
	LoLaTimeDebugTask(Scheduler* scheduler)
		: Task(0, TASK_FOREVER, scheduler, false)
	{

	}

	bool Setup(ILoLaClockSource* syncedClock, LoLaCryptoTokenSource* cryptoSeed)
	{
		SyncedClock = syncedClock;
		CryptoSeed = cryptoSeed;
		if (SyncedClock != nullptr && CryptoSeed != nullptr)
		{
			pinMode(OutputPin, OUTPUT);
			return true;
		}

		return false;
	}

	void OnLinkEstablished()
	{
		enable();

		//Task::delay(CryptoSeed->GetNextSwitchOverMillis());
	}

	void OnLinkLost()
	{
		disable();
	}

protected:

	bool Callback()
	{
		digitalWrite(OutputPin, HIGH);

		//Task::delay(CryptoSeed->GetNextSwitchOverMillis());

		digitalWrite(OutputPin, LOW);

		return true;
	}
};

class LoLaLinkTimedHopper : public ILoLaService
{
private:
	//Crypto Token.
	LoLaCryptoTokenSource	CryptoSeed;

	//Synced clock.
	ILoLaClockSource* SyncedClock = nullptr;

	//Channel manager.
	LoLaLinkChannelManager* ChannelManager = nullptr;

	//Crypto Encoder.
	LoLaCryptoEncoder* Encoder = nullptr;

	const uint32_t HopPeriod = LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS;

public:
	LoLaLinkTimedHopper(Scheduler* scheduler, ILoLaDriver* driver)
		: ILoLaService(scheduler, 0, driver)
	{

	}

	bool Setup(LoLaLinkChannelManager* channelManager)
	{
		ChannelManager = channelManager;
		SyncedClock = LoLaDriver->GetClockSource();
		Encoder = LoLaDriver->GetCryptoEncoder();
		if (SyncedClock != nullptr &&
			Encoder != nullptr &&
			ChannelManager != nullptr)
		{
			CryptoSeed.SetTOTPPeriod(HopPeriod);

			return true;
		}

		return false;
	}

	ITokenSource* GetTokenSource()
	{
		return &CryptoSeed;
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkTimedHopper (Cryp"));
		serial->print('|');
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		serial->print(F("Freq"));
#endif 
		serial->print(')');
	}
#endif // DEBUG_LOLA


	void OnLinkEstablished()
	{
		if (IsSetupOk())
		{
#ifndef LOLA_LINK_USE_TOKEN_HOP
			Encoder->SetToken(CryptoSeed.GetSeed());
#endif
			Enable();
			SetNextRunASAP();
		}
	}

	void OnLinkLost()
	{
		Disable();
	}

	bool Callback()
	{
		SetCurrent(CryptoSeed.GetToken(GetSyncMillis()));
		SetNextRunDelay(HopPeriod - (GetSyncMillis() % HopPeriod));

		return true;
	}

private:
	inline uint32_t GetSyncMillis()
	{
		return SyncedClock->GetSyncMicros() / (uint32_t)1000;
	}

	void SetCurrent(const uint32_t token)
	{
#ifdef LOLA_LINK_USE_TOKEN_HOP
		Encoder->SetToken(token);
#endif

#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		//Use last 8 bits of crypto token to generate a pseudo-random channel distribution.
		ChannelManager->SetNextHop((uint8_t)(token & 0xFF));
#endif
	}
};
#endif