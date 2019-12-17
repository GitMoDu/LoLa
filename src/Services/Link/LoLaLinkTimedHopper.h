// LoLaLinkTimedHopper.h

#ifndef _LOLA_TIMED_HOPPER_h
#define _LOLA_TIMED_HOPPER_h

#include <Services\ILoLaService.h>
#include <LoLaClock\ILoLaClockSource.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <LoLaCrypto\LoLaCryptoTokenSource.h>
#include <Services\Link\LoLaLinkChannelManager.h>

//This doesn't need to derive from ILoLaService!
template <const uint32_t HopPeriod>
class LoLaLinkTimedHopper : public Task
{
private:
	//Synced clock.
	ILoLaClockSource* SyncedClock = nullptr;

	//Crypto Encoder.
	LoLaCryptoEncoder* Encoder = nullptr;

	//
	ILoLaDriver* LoLaDriver;

public:
	LoLaLinkTimedHopper(Scheduler* scheduler, ILoLaDriver* driver, ILoLaClockSource* syncedClock)
		: Task(HopPeriod, TASK_FOREVER, scheduler, false)
	{
		SyncedClock = syncedClock;
		LoLaDriver = driver;
		Encoder = LoLaDriver->GetCryptoEncoder();
	}

	bool Setup()
	{
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		if (SyncedClock != nullptr &&
			Encoder != nullptr)
		{
			return true;
		}
#else
		return true;
#endif

		return false;
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

	virtual void OnLinkStatusChanged()
	{
		if (LoLaDriver->HasLink())
		{
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
			enable();
#endif
		}
		else
		{
			disable();
		}
	}

	bool Callback()
	{
		//SetCurrent(CryptoSeed.GetToken(GetSyncMillis()));
		//SetNextRunDelay(HopPeriod - (GetSyncMillis() % HopPeriod));

		return false;
	}

private:
	void SetCurrent(const uint32_t token)
	{

		//Use last 8 bits of crypto token to generate a pseudo-random channel distribution.
		//SetNextHop((uint8_t)(token & 0xFF));
	}
};
#endif