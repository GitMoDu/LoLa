// Duplexes.h

#ifndef _DUPLEXES_h
#define _DUPLEXES_h

#include "..\Link\IDuplex.h"
#include "..\Link\LoLaLinkDefinition.h"

/// <summary>
/// Fixed full duplex, is always in slot.
/// </summary>
//template<const uint16_t DuplexPeriodMicros>
class FullDuplex : public virtual IDuplex
{
public:
	virtual const bool IsInRange(const uint32_t startTimestamp, const uint32_t endTimestamp) final
	{
		return true;
	}

	virtual const uint32_t GetRange() final
	{
		return DUPLEX_FULL;
	}
};

/// <summary>
/// Template timed dual slot duplex.
/// Suitable for Point-to-Point applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros">[2;65535]</typeparam>
/// <typeparam name="IsOddSlot">First or Second half of duplex.</typeparam>
/// <typeparam name="DeadZoneMicros"></typeparam>
template<const uint16_t DuplexPeriodMicros,
	const uint16_t SwitchOver,
	const bool IsOddSlot,
	const uint16_t DeadZoneMicros = 0>
class TemplateHalfDuplex : public IDuplex
{
public:
	TemplateHalfDuplex() : IDuplex()
	{}

public:
	virtual const bool IsInRange(const uint32_t startTimestamp, const uint32_t endTimestamp) final
	{
		const uint_least16_t startRemainder = startTimestamp % DuplexPeriodMicros;
		const uint_least16_t endRemainder = endTimestamp % DuplexPeriodMicros;

		if (IsOddSlot)
		{
			return (startRemainder >= (SwitchOver + DeadZoneMicros)) &&
				(startRemainder < (DuplexPeriodMicros - DeadZoneMicros)) &&
				(endRemainder >= (SwitchOver + DeadZoneMicros)) &&
				(endRemainder < (DuplexPeriodMicros - DeadZoneMicros));
		}
		else
		{
			return (startRemainder >= DeadZoneMicros) &&
				(startRemainder < (SwitchOver - DeadZoneMicros)) &&
				(endRemainder >= DeadZoneMicros) &&
				(endRemainder < (SwitchOver - DeadZoneMicros));
		}
	}

	virtual const uint32_t GetRange() final
	{
		return SwitchOver - (2 * DeadZoneMicros);
	}
};

/// <summary>
/// Static dual slot duplex.
/// Suitable for Point-to-Point applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros">[2;65535]</typeparam>
/// <typeparam name="IsOddSlot">First or Second half of duplex.</typeparam>
/// <typeparam name="DeadZoneMicros"></typeparam>
template<const uint16_t DuplexPeriodMicros,
	const bool IsOddSlot,
	const uint16_t DeadZoneMicros = 0>
class HalfDuplex : public TemplateHalfDuplex<
	DuplexPeriodMicros,
	DuplexPeriodMicros / 2,
	IsOddSlot,
	DeadZoneMicros>
{};


/// <summary>
/// Static asymmetric dual slot duplex.
/// Suitable for one-side-biased applications,
/// such as streaming audio/video.
/// </summary>
/// <typeparam name="DuplexPeriodMicros"></typeparam>
/// <typeparam name="LongSlotRatio"></typeparam>
/// <typeparam name="IsLongSlot"></typeparam>
/// <typeparam name="DeadZoneMicros"></typeparam>
template<const uint16_t DuplexPeriodMicros,
	const uint8_t LongSlotRatio,
	const bool IsLongSlot,
	const uint16_t DeadZoneMicros = 0>
class HalfDuplexAsymmetric : public TemplateHalfDuplex<
	DuplexPeriodMicros,
	((uint32_t)LongSlotRatio* DuplexPeriodMicros) / UINT8_MAX,
	IsLongSlot,
	DeadZoneMicros>
{};

/// <summary>
/// Time Division Duplex.
/// Each division is a "Slot", WiFi 6 style.
/// Slots are dynamic and evenly distributed.
/// Suitable for WAN applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros"></typeparam>
/// <typeparam name="DeadZoneMicros"></typeparam>
template<const uint16_t DuplexPeriodMicros,
	const uint16_t DeadZoneMicros = 0>
class SlottedDuplex : public virtual IDuplex
{
private:
	uint8_t Slots = 1;
	uint8_t Slot = 0;

public:
	SlottedDuplex()
	{}

	const bool SetTotalSlots(const uint8_t slots)
	{
		if (slots > 0)
		{
			Slots = slots;

			UpdateTimings();

			return true;
		}
		else
		{
			return false;
		}
	}

	const bool SetSlot(const uint8_t slot)
	{
		if (slot < Slots)
		{
			Slot = slot;
			UpdateTimings();
			return true;
		}
		else
		{
			return false;
		}
	}

public:
	virtual const bool IsInRange(const uint32_t startTimestamp, const uint32_t endTimestamp) final
	{
		const uint_least16_t startRemainder = startTimestamp % DuplexPeriodMicros;
		const uint_least16_t endRemainder = endTimestamp % DuplexPeriodMicros;

		return (startRemainder >= (Start + DeadZoneMicros)) &&
			(startRemainder < (End - DeadZoneMicros)) &&
			(endRemainder >= (Start + DeadZoneMicros)) &&
			(endRemainder < (End - DeadZoneMicros));
	}

	virtual const bool IsFullDuplex() final
	{
		return false;
	}

private:
	uint_least16_t Start = 0;
	uint_least16_t End = DuplexPeriodMicros / Slots;

	void UpdateTimings()
	{
		Start = ((uint32_t)Slot * DuplexPeriodMicros) / Slots;
		End = ((uint32_t)(Slot + 1) * DuplexPeriodMicros) / Slots;
	}

	virtual const uint32_t GetRange() final
	{
		return DuplexPeriodMicros / Slots - (2 * DuplexPeriodMicros);
	}
};


#endif