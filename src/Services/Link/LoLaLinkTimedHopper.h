// LoLaLinkTimedHopper.h

#ifndef _LOLA_TIMED_HOPPER_h
#define _LOLA_TIMED_HOPPER_h

#include <LoLaCrypto\TinyCRC.h>
#include <Services\ILoLaService.h>
#include <ILoLa.h>
#include <LoLaCrypto\ITokenSource.h>
#include <Services\Link\LoLaLinkDefinitions.h>


class LoLaLinkTimedHopper : public ILoLaService
{
private:
	//Crypto Token.
	LoLaCryptoTokenSource	CryptoSeed;

	//Synced clock.
	ClockSource* SyncedClock = nullptr;

	const uint32_t HopPeriod = LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS;

public:
	LoLaLinkTimedHopper(Scheduler* scheduler, ILoLa* loLa)
		: ILoLaService(scheduler, 0, loLa)
	{

	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkTimedHopper ("));
#ifdef LOLA_USE_CRYPTO_TOKEN
		serial->print(F("Cryp"));
#endif 
		serial->print('|');
#ifdef LOLA_USE_FREQUENCY_HOP
		serial->print(F("Freq"));
#endif 
		serial->print(')');
	}
#endif // DEBUG_LOLA

	void OnLinkEstablished()
	{
		if (IsSetupOk())
		{
			Enable();
			SetNextRunDelay(CryptoSeed.GetNextSwitchOverMillis());
		}
	}

	void OnLinkLost()
	{
		Disable();
	}

	bool Callback()
	{
		Serial.print(F("TimedHop: "));
		Serial.println(CryptoSeed.GetToken());
#ifdef USE_FREQUENCY_HOP
#endif
		SetNextRunDelay(CryptoSeed.GetNextSwitchOverMillis());
	}
	//#ifdef USE_FREQUENCY_HOP
	//		if (IsWarmUpTime)
	//		{
	//			SetNextRunDelay(LOLA_LINK_FREQUENCY_HOPPER_WARMUP_MILLIS);
	//			IsWarmUpTime = false;
	//			return true;
	//		}
	//		else
	//		{
	//			GetLoLa()->SetChannel(GetHopChannel());
	//
	//			//Try to sync the next run based on the synced clock.
	//			SetNextRunDelay(GetNextSwitchOverDelay());
	//			return true;
	//		}
	//#else
	//		Disable();
	//#endif
	//
	//		return false;
	//	}
public:
	bool Setup(ClockSource* syncedClock)
	{
		SyncedClock = syncedClock;
		if (CryptoSeed.Setup(SyncedClock))
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
};
#endif