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
	virtual const bool IsInSlot(const uint32_t timestamp) final
	{
		return true;
	}

	virtual const bool IsFullDuplex() final
	{
		return true;
	}
};

/// <summary>
/// Template timed dual slot duplex.
/// Suitable for Point-to-Point applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros">[2;65535]</typeparam>
/// <typeparam name="IsOddSlot">First or Second half of duplex.</typeparam>
template<const uint16_t DuplexPeriodMicros,
	const uint16_t SwitchOver,
	const bool IsOddSlot>
class TemplateHalfDuplex : public IDuplex
{
public:
	TemplateHalfDuplex() : IDuplex()
	{}

public:
	virtual const bool IsInSlot(const uint32_t timestamp) final
	{
		const uint16_t remainder = timestamp % DuplexPeriodMicros;

		// Check if remainder is before or after the switchover time.
		if (IsOddSlot)
		{
			return remainder >= SwitchOver;
		}
		else
		{
			return remainder < SwitchOver;
		}
	}

	virtual const bool IsFullDuplex() final
	{
		return false;
	}
};

/// <summary>
/// Static dual slot duplex.
/// Suitable for Point-to-Point applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros">[2;65535]</typeparam>
/// <typeparam name="IsOddSlot">First or Second half of duplex.</typeparam>
template<const uint16_t DuplexPeriodMicros,
	const bool IsOddSlot>
class HalfDuplex : public TemplateHalfDuplex<
	DuplexPeriodMicros,
	DuplexPeriodMicros / 2,
	IsOddSlot>
{};


/// <summary>
/// Static asymmetric dual slot duplex.
/// Suitable for one-side-biased applications,
/// such as streaming audio/video.
/// </summary>
/// <typeparam name="DuplexPeriodMicros"></typeparam>
/// <typeparam name="LongSlotRatio"></typeparam>
/// <typeparam name="IsLongSlot"></typeparam>
template<const uint16_t DuplexPeriodMicros,
	const uint8_t LongSlotRatio,
	const bool IsLongSlot>
class HalfDuplexAsymmetric : public TemplateHalfDuplex<
	DuplexPeriodMicros,
	((uint32_t)LongSlotRatio* DuplexPeriodMicros) / UINT8_MAX,
	IsLongSlot>
{};

/// <summary>
/// Time Division Duplex.
/// Each division is a "Slot", WiFi 6 style.
/// Slots are dynamic and evenly distributed.
/// Suitable for WAN applications.
/// </summary>
/// <typeparam name="DuplexPeriodMicros"></typeparam>
template<const uint16_t DuplexPeriodMicros>
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
			return true;
		}
		else
		{
			return false;
		}
	}

	const bool SetSlot(const uint8_t slot)
	{
		Slot = slot;
		return slot < Slots;
	}

public:
	virtual const bool IsInSlot(const uint32_t timestamp) final
	{
		const uint16_t remainder = timestamp % DuplexPeriodMicros;

		const uint16_t start = ((uint32_t)Slot * DuplexPeriodMicros) / Slots;

		const uint16_t end = ((uint32_t)(Slot + 1) * DuplexPeriodMicros) / Slots;

		return remainder >= start && remainder < end;
	}

	virtual const bool IsFullDuplex() final
	{
		return false;
	}
};
#endif