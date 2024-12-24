#ifndef _CLOCK_LAYOUT_CALCULATOR_h
#define _CLOCK_LAYOUT_CALCULATOR_h

#include <stdint.h>

/// <summary>
/// </summary>
/// <typeparam name="width"></typeparam>
/// <typeparam name="height"></typeparam>
template<const uint8_t width, const uint8_t height>
class ClockLayoutCalculator
{
public:
	static constexpr uint8_t CharCount() { return 6; }
	static constexpr uint8_t SpacerCount() { return 2; }
	static constexpr uint8_t SpacerWidth() { return 3; }
	static constexpr uint8_t SpacerMargin() { return 1; }
	static constexpr uint8_t KerningCount() { return 3; }

	static constexpr uint8_t SpacersWidth() { return SpacerWidth() * SpacerCount(); }

	static constexpr uint8_t CharsPlusKerningsWidth() { return width - SpacersWidth(); }
	static constexpr uint8_t TargetFontWidth() { return ((width - SpacersWidth()) / CharCount()); }
	static constexpr uint8_t FontKerning() { return FontStyle::GetKerning(FontStyle::KERNING_RATIO_DEFAULT, TargetFontWidth()); }

	static constexpr uint8_t SpacersPlusKerningWidth() { return (SpacersWidth() + (FontKerning() * 2)) * 2; }

	static constexpr uint8_t KerningsWidth() { return FontKerning() * KerningCount(); }

	static constexpr uint8_t FontWidth() 
	{ 
		return Smallest(FontStyle::GetFontWidth(FontStyle::WIDTH_RATIO_DEFAULT, height), (CharsPlusKerningsWidth() - KerningsWidth() - 4) / CharCount());
		//return (CharsPlusKerningsWidth() - KerningsWidth() - 4) / CharCount();
	}

	static constexpr uint8_t FontHeight()
	{
		return Smallest(height, FontStyle::GetFontHeight(FontStyle::WIDTH_RATIO_DEFAULT, FontWidth()));
	}

	static constexpr uint8_t EffectiveWidth() { return ((FontWidth() + FontKerning() + FontWidth()) * 3) + ((SpacerWidth()) * 2); }
	static constexpr uint8_t MarginStart() { return (width - EffectiveWidth()) / 2; }

private:
	static constexpr uint8_t Smallest(const uint8_t a, const uint8_t b) { return ((b >= a) * a) | ((b < a) * b); }
};
#endif