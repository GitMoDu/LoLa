// FixedHoppers.h

#ifndef _FIXED_HOPPERS_h
#define _FIXED_HOPPERS_h

#include "..\Link\IChannelHop.h"

/// <summary>
/// For use when PHY layer doesn't support multiple channels.
/// </summary>
class NoHopNoChannel final : public virtual IChannelHop
{
public:
	NoHopNoChannel() : IChannelHop()
	{}

public:
	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) final
	{
		return true;
	}
};


/// <summary>
/// Permanent Fixed channel.
/// </summary>
template<const uint8_t PermanentChannel>//= FixedChannel
class FixedChannelNoHop final : public virtual IChannelHop
{
public:
	FixedChannelNoHop() : IChannelHop()
	{}

	virtual const uint8_t GetBroadcastChannel() final
	{
		return PermanentChannel;
	}

	virtual const uint8_t GetFixedChannel() final
	{
		return PermanentChannel;
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) final
	{
		return true;
	}
};


/// <summary>
/// Dynamic channel during linking.
/// Fixed channel during link, no hopping done.
/// </summary>
class NoHopDynamicChannel final : public virtual IChannelHop
{
private:
	volatile uint8_t FixedChannel = 0;

public:
	NoHopDynamicChannel() : IChannelHop()
	{}

	virtual const uint8_t GetBroadcastChannel() final
	{
		return FixedChannel;
	}

	virtual const uint8_t GetFixedChannel() final
	{
		return FixedChannel;
	}

	virtual void SetFixedChannel(const uint8_t channel) final
	{
		FixedChannel = channel;
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) final
	{
		return true;
	}
};

/// <summary>
/// Dynamic channel, no hopping done.
/// </summary>
template<const uint8_t BroadcastChannel>
class NoHopDynamicChannelFixedBroadcast : public virtual IChannelHop
{
private:
	volatile uint8_t FixedChannel = 0;

public:
	NoHopDynamicChannelFixedBroadcast() : IChannelHop()
	{}

	virtual const uint8_t GetBroadcastChannel() final
	{
		return BroadcastChannel;
	}

	virtual const uint8_t GetFixedChannel() final
	{
		return FixedChannel;
	}

	virtual void SetFixedChannel(const uint8_t channel) final
	{
		FixedChannel = channel;
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) final
	{
		return true;
	}
};
#endif