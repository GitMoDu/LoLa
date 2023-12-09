// PreLinkDuplex.h

#ifndef _PRE_LINK_DUPLEX_h
#define _PRE_LINK_DUPLEX_h

#include <stdint.h>

/// <summary>
/// Constructor timed dual slot duplex.
/// </summary>
/// <typeparam name="IsOddSlot">First or Second half of duplex.</typeparam>
template<const bool IsOddSlot>
class PreLinkDuplex
{
protected:
	const uint_fast16_t PreLinkDuplexPeriodMicros;
	const uint_fast16_t DuplexStart;
	const uint_fast16_t DuplexEnd;

protected:
	static constexpr uint16_t GetPreLinkDuplexPeriod(const uint16_t duplexPeriodMicros)
	{
		return duplexPeriodMicros * 2;
	}

	template<const bool IsOdd>
	static constexpr uint16_t GetPreLinkDuplexStart(const uint16_t duplexPeriodMicros)
	{
		return (((uint8_t)IsOdd) * duplexPeriodMicros)
			+ (((uint8_t)!IsOdd) * 0);
	}

	template<const bool IsOdd>
	static constexpr uint16_t GetPreLinkDuplexEnd(const uint16_t duplexPeriodMicros)
	{
		return (((uint8_t)IsOdd) * (duplexPeriodMicros + (duplexPeriodMicros / 2)))
			+ (((uint8_t)!IsOdd) * (duplexPeriodMicros / 2));
	}

public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;32,767]</param>
	PreLinkDuplex(const uint16_t duplexPeriodMicros)
		: PreLinkDuplexPeriodMicros(GetPreLinkDuplexPeriod(duplexPeriodMicros))
		, DuplexStart(GetPreLinkDuplexStart<IsOddSlot>(duplexPeriodMicros))
		, DuplexEnd(GetPreLinkDuplexEnd<IsOddSlot>(duplexPeriodMicros))
	{}

public:
	const bool IsInRange(const uint32_t timestamp, const uint16_t duration) const
	{
		const uint_fast16_t startRemainder = timestamp % PreLinkDuplexPeriodMicros;

		return startRemainder >= DuplexStart
			&& startRemainder < DuplexEnd
			&& (duration < (DuplexEnd - startRemainder));
	}
};


class PreLinkMasterDuplex : public PreLinkDuplex<false>
{
public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;32,767]</param>
	PreLinkMasterDuplex(const uint16_t duplexPeriodMicros)
		: PreLinkDuplex<false>(duplexPeriodMicros)
	{}
};

class PreLinkSlaveDuplex : public PreLinkDuplex<true>
{
private:
	using BaseClass = PreLinkDuplex<true>;

private:
	uint_fast16_t FollowerOffset = 0;

public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;32,767]</param>
	PreLinkSlaveDuplex(const uint16_t duplexPeriodMicros)
		: BaseClass(duplexPeriodMicros)
	{}

public:
	const uint16_t GetFollowerOffset()
	{
		return FollowerOffset;
	}

	void OnReceivedSync(const uint32_t sentTimestamp)
	{
		FollowerOffset = sentTimestamp % BaseClass::PreLinkDuplexPeriodMicros;
	}
};
#endif