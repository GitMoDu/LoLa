// IChannelHop.h

#ifndef _I_CHANEL_HOP_h
#define _I_CHANEL_HOP_h

#include <stdint.h>
#include "..\Clock\SynchronizedClock.h"

class IChannelHop
{
public:
	class IHopListener
	{
	public:
		virtual void OnChannelHopTime() {}
	};

public:
	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) { return false; }

#pragma region General Channel Interfaces
	/// <summary>
	/// Fixed broadcast channel.
	/// </summary>
	virtual const uint8_t GetBroadcastChannel() { return 0; }

	/// <summary>
	/// Fixed channel.
	/// </summary>
	virtual const uint8_t GetFixedChannel() { return 0; }

	virtual void SetFixedChannel(const uint8_t channel) {}
#pragma endregion

#pragma region Hopper Interfaces
	virtual const bool IsHopper() { return false; }

	virtual const uint32_t GetHopIndex(const uint32_t timestamp) { return 0; }

	virtual const uint32_t GetTimedHopIndex() { return 0; }

	virtual void OnLinkStarted() { }

	virtual void OnLinkStopped() { }
#pragma endregion
};

#endif