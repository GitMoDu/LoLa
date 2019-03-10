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

	//Crypto Encoder.
	LoLaCryptoEncoder* Encoder = nullptr;

	const uint32_t HopPeriod = LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS;

	//Helper.
	uint32_t TokenHelper = 0;

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
		SetNextRunDelay(CryptoSeed.GetNextSwitchOverMillis());

		TokenHelper = CryptoSeed.GetToken();

		Encoder->SetToken(TokenHelper);
#ifdef USE_FREQUENCY_HOP
		//GetLoLa()->SetChannel(GetHopChannel());
#endif
	}

public:
	bool Setup(ClockSource* syncedClock)
	{
		SyncedClock = syncedClock;
		Encoder = GetLoLa()->GetCryptoEncoder();
		if (Encoder != nullptr &&
			CryptoSeed.Setup(SyncedClock))
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