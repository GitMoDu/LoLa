// FixedHoppers.h

#ifndef _FIXED_HOPPERS_h
#define _FIXED_HOPPERS_h

#include "ChannelHoppers/IChannelHop.h"

template<const uint8_t BroadcastChannel,
	const uint8_t PermanentChannel>
class TemplateFixedChannelNoHop : public virtual IChannelHop
{
public:
	TemplateFixedChannelNoHop() : IChannelHop()
	{}

	const bool Setup(IChannelHop::IHopListener* listener, LinkClock* linkClock) final
	{
		return listener != nullptr && linkClock != nullptr;
	}

	const uint8_t GetChannel() final
	{
		return PermanentChannel;
	}

	void SetChannel(const uint8_t channel) final
	{}

	const uint32_t GetHopPeriod() final
	{
		return IChannelHop::NOT_A_HOPPER;
	}

	const uint32_t GetHopIndex(const uint32_t timestamp)  final { return 0; }

	const uint32_t GetTimedHopIndex() final { return 0; }

	void OnLinkStarted() final { }

	void OnLinkStopped() final { }
};

/// <summary>
/// Permanent Fixed channel.
/// </summary>
/// <typeparam name="PermanentChannel">Single channel used for all purposes.</typeparam>
template<const uint8_t PermanentChannel>
class NoHopFixedChannel final : public virtual TemplateFixedChannelNoHop<PermanentChannel, PermanentChannel>
{
public:
	NoHopFixedChannel() : IChannelHop()
	{}
};

/// <summary>
/// For use when PHY layer doesn't support multiple channels.
/// </summary>
class NoHopNoChannel final : public TemplateFixedChannelNoHop<0, 0>
{
public:
	NoHopNoChannel() : TemplateFixedChannelNoHop<0, 0>()
	{}
};
#endif