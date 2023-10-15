// FixedHoppers.h

#ifndef _FIXED_HOPPERS_h
#define _FIXED_HOPPERS_h

#include <IChannelHop.h>

template<const uint8_t BroadcastChannel,
	const uint8_t PermanentChannel>
class TemplateFixedChannelNoHop : public virtual IChannelHop
{
public:
	TemplateFixedChannelNoHop() : IChannelHop()
	{}

	virtual void SetChannel() final
	{}

	virtual const uint8_t GetChannel() final
	{
		return PermanentChannel;
	}

	virtual const uint32_t GetHopPeriod() final
	{
		return IChannelHop::NOT_A_HOPPER;
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, IRollingTimestamp* rollingTimestamp) final
	{
		return listener != nullptr && rollingTimestamp != nullptr;
	}
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