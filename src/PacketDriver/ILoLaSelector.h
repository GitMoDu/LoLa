// ILoLaSelector.h

#ifndef _ILOLA_SELECTOR_h
#define _ILOLA_SELECTOR_h

class ITransmitPowerSelector
{
public:
	virtual uint8_t GetTransmitPowerNormalized() { return UINT8_MAX / 2; }
};

class IChannelSelector
{
public:
	IChannelSelector() {}

	virtual uint8_t GetChannel() { return 0; }
};
#endif