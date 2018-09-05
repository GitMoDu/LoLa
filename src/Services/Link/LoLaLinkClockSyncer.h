// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>
#include <RingBufCPP.h>

class LoLaLinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;
	boolean Synced = false;

public:
	bool Setup(ClockSource * syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	bool IsSynced()
	{
		return Synced;
	}

	void Reset()
	{
		Synced = false;
		if (SyncedClock != nullptr)
		{
			SyncedClock->SetRandom();
		}
	}

} ClockSyncer;

#endif

