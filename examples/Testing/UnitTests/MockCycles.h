// MockCycles.h

#ifndef _MOCK_h
#define _MOCK_h

#include <ICycles.h>

/// <summary>
/// Simulates a full 16 bit timer out of the Arduino micros().
/// 1 Cycle = 1 us.
/// </summary>
class Mock16BitCycles final : public virtual ICycles
{
public:
	Mock16BitCycles() : ICycles()
	{}

public:
	virtual const uint32_t GetCycles() final
	{
		return micros() & UINT16_MAX;
	}

	virtual const uint32_t GetCyclesOverflow() final
	{
		return UINT16_MAX;
	}

	virtual const uint32_t GetCyclesOneSecond() final
	{
		return ONE_SECOND_MICROS;
	}

	virtual void StartCycles() final
	{}
	virtual void StopCycles() final
	{}
};

/// <summary>
/// Simulates a fast 32 bit timer out of the Arduino micros().
/// 1 Cycle = 0.5 us.
/// </summary>
class Mock32BitCycles final : public virtual ICycles
{
public:
	Mock32BitCycles() : ICycles()
	{}

public:
	virtual const uint32_t GetCycles() final
	{
		return micros() * 2;
	}

	virtual const uint32_t GetCyclesOverflow() final
	{
		return UINT32_MAX;
	}

	virtual const uint32_t GetCyclesOneSecond() final
	{
		return ONE_SECOND_MICROS * 2;
	}

	virtual void StartCycles() final
	{}
	virtual void StopCycles() final
	{}
};
#endif