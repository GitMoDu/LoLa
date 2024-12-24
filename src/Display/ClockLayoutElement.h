// ClockLayoutElement.h

#ifndef _CLOCK_LAYOUT_ELEMENT_h
#define _CLOCK_LAYOUT_ELEMENT_h

#include <ArduinoGraphicsDrawer.h>
#include "ClockLayoutCalculator.H"

template<const uint8_t x, const uint8_t y,
	const uint8_t width, const uint8_t height>
class ClockLayoutElement : public FontLayoutElement<x, y, width, height, ClockLayoutCalculator<width, height>::FontWidth(), ClockLayoutCalculator<width, height>::FontHeight(), ClockLayoutCalculator<width, height>::FontKerning()>
{
private:
	using LayoutCalculator = ClockLayoutCalculator<width, height>;

	using BaseClass = FontLayoutElement<x, y, width, height, ClockLayoutCalculator<width, height>::FontWidth(), ClockLayoutCalculator<width, height>::FontHeight(), ClockLayoutCalculator<width, height>::FontKerning()>;

public:
	using BaseClass::FontWidth;
	using BaseClass::FontHeight;
	using BaseClass::FontKerning;

	static constexpr uint8_t DigitHours1X()
	{
		return x + LayoutCalculator::MarginStart();
	}

	static constexpr uint8_t DigitHours2X()
	{
		return DigitHours1X() + FontWidth() + FontKerning();
	}

	static constexpr uint8_t Separator1X()
	{
		return DigitHours2X() + FontWidth() + 1;
	}

	static constexpr uint8_t DigitMinutes1X()
	{
		return Separator1X() + (LayoutCalculator::SpacerMargin() * 2);
	}

	static constexpr uint8_t DigitMinutes2X()
	{
		return DigitMinutes1X() + FontWidth() + FontKerning();
	}

	static constexpr uint8_t Separator2X()
	{
		return DigitMinutes2X() + FontWidth() + 1;
	}

	static constexpr uint8_t DigitSeconds1X()
	{
		return Separator2X() + (LayoutCalculator::SpacerMargin() * 2);
	}

	static constexpr uint8_t DigitSeconds2X()
	{
		return DigitSeconds1X() + FontWidth() + FontKerning();
	}
};
#endif