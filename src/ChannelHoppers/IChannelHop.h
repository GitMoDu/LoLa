// IChannelHop.h

#ifndef _I_CHANEL_HOP_h
#define _I_CHANEL_HOP_h

#include <stdint.h>

#include "Clock\LinkClock.h"

/// <summary>
/// Interface for generic channel hopper/manager.
/// </summary>
class IChannelHop
{
public:
	static constexpr uint16_t NOT_A_HOPPER = 0;

public:
	class IHopListener
	{
	public:
		virtual void OnChannelHopTime() {}
	};

public:
	virtual const bool Setup(IChannelHop::IHopListener* listener, LinkClock* linkClock) { return false; }

	/// General Channel Interfaces
public:
	/// <summary>
	/// Current channel.
	/// </summary>
	virtual const uint8_t GetChannel() { return 0; }

	virtual void SetChannel(const uint8_t channel) {}
	///

	/// Hopper Interfaces
public:
	virtual const uint32_t GetHopPeriod() { return NOT_A_HOPPER; }

	virtual const uint32_t GetHopIndex(const uint32_t timestamp) { return 0; }

	virtual const uint32_t GetTimedHopIndex() { return 0; }

	virtual void OnLinkStarted() { }
	virtual void OnLinkStopped() { }
	///
};

#endif