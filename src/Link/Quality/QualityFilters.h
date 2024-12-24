// QualityFilters.h

#ifndef _QUALITY_FILTERS_h
#define _QUALITY_FILTERS_h

#include <stdint.h>

/// <summary>
/// Template EmaFilter with scale and last value tracking.
/// </summary>
/// <typeparam name="FilterScale">Unitless scale for filter weight.</typeparam>
/// <typeparam name="uint_t">Unsigned value type.</typeparam>
/// <typeparam name="temp_t">Unsigned state type.</typeparam>
template <const uint8_t FilterScale, typename uint_t, typename temp_t>
class TemplateEmaFilter
{
private:
	/// <summary>
	/// Filter order, derived from unitless filter scale.
	/// </summary>
	constexpr static uint_t K = ((uint16_t)FilterScale * 8) / UINT8_MAX;

	/// <summary>
	/// Fixed point representation of one half, used for rounding.
	/// </summary>
	constexpr static uint_t Half = 1 << (K - 1);

private:
	temp_t State = 0;
	uint_t LastValue = 0;

public:
	void Clear(const uint_t clearValue = 0)
	{
		State = clearValue;
		LastValue = State;
	}

	/// <summary>
	/// Update the filter with the given input and return the filtered output.
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	const uint_t Step(uint_t input)
	{
		State += input;
		const uint_t output = (State + Half) >> 1;
		State -= output;

		LastValue = output;

		return output;
	}

	const uint_t Get() const
	{
		return LastValue;
	}
};

/// <summary>
/// Template EMA filter for uint8_t.
/// </summary>
/// <typeparam name="FilterScale">[0:UINT8_MAX]</typeparam>
template<const uint8_t FilterScale>
class EmaFilter8 : public TemplateEmaFilter<FilterScale, uint8_t, uint16_t> {};

/// <summary>
/// Template EMA filter for uint16_t.
/// </summary>
/// <typeparam name="FilterScale">[0:UINT8_MAX]</typeparam>
template<const uint8_t FilterScale>
class EmaFilter16 : public TemplateEmaFilter<FilterScale, uint16_t, uint32_t> {};
#endif