// PendingActionsTracker.h

#ifndef _PENDINGACTIONSTRACKER_h
#define _PENDINGACTIONSTRACKER_h

#include <BitTracker.h>

class PendingActionsTracker : TemplateBitTracker<2>
{
private:
	const uint8_t NeedRXIndex = 0;
	const uint8_t NeedPowerIndex = 1;
	//const uint8_t NeedsChannelIndex = 2;

public:
	bool GetRX()
	{
		return IsBitSet(NeedRXIndex);
	}

	bool GetPower()
	{
		return IsBitSet(NeedPowerIndex);
	}

	bool GetChannel()
	{
		return GetRX();
		//return IsBitSet(NeedsChannelIndex);
	}

	void SetRX()
	{
		SetBit(NeedRXIndex);
	}

	void SetPower()
	{
		SetBit(NeedPowerIndex);
	}

	void SetChannel()
	{
		SetRX();
		//SetBit(NeedsChannelIndex);
	}

	void ClearRX()
	{
		ClearBit(NeedRXIndex);
	}

	void ClearPower()
	{
		ClearBit(NeedPowerIndex);
	}

	void ClearChannel()
	{
		ClearRX();
	}

	void Clear()
	{
		ClearAll();
	}

public:
	PendingActionsTracker() : TemplateBitTracker<2>()
	{
	}

};



#endif

