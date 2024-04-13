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
	const uint32_t PreLinkDuplexPeriodMicros;
	const uint32_t DuplexStart;
	const uint32_t DuplexEnd;

protected:
	static constexpr uint32_t GetPreLinkDuplexPeriod(const uint16_t duplexPeriodMicros)
	{
		return (uint32_t)duplexPeriodMicros * 4;
	}

	template<const bool IsOdd>
	static constexpr uint32_t GetPreLinkDuplexStart(const uint16_t duplexPeriodMicros)
	{
		return (((uint32_t)IsOdd) * 2 * duplexPeriodMicros)
			+ (((uint32_t)!IsOdd) * duplexPeriodMicros);
	}

	template<const bool IsOdd>
	static constexpr uint32_t GetPreLinkDuplexEnd(const uint16_t duplexPeriodMicros)
	{
		return (((uint32_t)IsOdd) * 3 * duplexPeriodMicros)
			+ (((uint32_t)!IsOdd) * 2 * duplexPeriodMicros);
	}

public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;UINT16_MAX]</param>
	PreLinkDuplex(const uint16_t duplexPeriodMicros)
		: PreLinkDuplexPeriodMicros(GetPreLinkDuplexPeriod(duplexPeriodMicros))
		, DuplexStart(GetPreLinkDuplexStart<IsOddSlot>(duplexPeriodMicros))
		, DuplexEnd(GetPreLinkDuplexEnd<IsOddSlot>(duplexPeriodMicros))
	{}

public:
	const bool IsInRange(const uint32_t timestamp, const uint16_t duration) const
	{
		const uint32_t startRemainder = timestamp % PreLinkDuplexPeriodMicros;

		return startRemainder >= DuplexStart
			&& startRemainder < DuplexEnd
			&& (duration <= (DuplexEnd - startRemainder));
	}
};

class PreLinkMasterDuplex : public PreLinkDuplex<false>
{
public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;UINT16_MAX]</param>
	PreLinkMasterDuplex(const uint16_t duplexPeriodMicros)
		: PreLinkDuplex<false>(duplexPeriodMicros)
	{}
};

class PreLinkSlaveDuplex : public PreLinkDuplex<true>
{
private:
	using BaseClass = PreLinkDuplex<true>;

private:
	uint32_t FollowerOffset = 0;

public:
	/// <summary>
	/// </summary>
	/// <param name="DuplexPeriodMicros">[2;UINT16_MAX]</param>
	PreLinkSlaveDuplex(const uint16_t duplexPeriodMicros)
		: BaseClass(duplexPeriodMicros)
	{}

public:
	const uint32_t GetFollowerOffset()
	{
		return FollowerOffset;
	}

	void OnReceivedSync(const uint32_t sentTimestamp)
	{
		FollowerOffset = sentTimestamp % BaseClass::PreLinkDuplexPeriodMicros;
	}
};
#endif