// ILoLaSelector.h

#ifndef _ILOLA_SELECTOR_h
#define _ILOLA_SELECTOR_h


class IChannelSelector
{
public:
	virtual uint8_t GetChannel() { return UINT8_MAX / 2; }
	virtual void ResetChannel() {}
};

class ITransmitPowerSelector
{
public:
	virtual uint8_t GetTransmitPower() { return UINT8_MAX / 2; }
};



#endif

