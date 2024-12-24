// LinkDisplayLayout.h

#ifndef _LINK_DISPLAY_LAYOUT_h
#define _LINK_DISPLAY_LAYOUT_h

#include "ClockLayoutElement.h"

template<uint8_t x, uint8_t y,
	uint8_t width, uint8_t height>
class LinkDisplayLayout : public LayoutElement<x, y, width, height>
{
public:
	using LayoutElement<x, y, width, height>::X;
	using LayoutElement<x, y, width, height>::Y;
	using LayoutElement<x, y, width, height>::Width;
	using LayoutElement<x, y, width, height>::Height;

public:
	static constexpr uint8_t RssiLinesCount() { return 4; }
	static constexpr uint8_t RssiPoleHeight() { return 4; }

private:
	static constexpr uint8_t RssiWidth() { return 15; }
	static constexpr uint8_t RssiLinesHeight() { return (((uint16_t)RssiLinesCount()) * 2); }
	static constexpr uint8_t RssiHeight() { return RssiLinesHeight() + 1 + RssiPoleHeight(); }
	static constexpr uint8_t QualityWidth() { return 3; }
	static constexpr uint8_t QualityHeight() { return RssiHeight() - 4; }
	static constexpr uint8_t IoWidth() { return  (((RssiWidth() - 1) / 2) - 2); }
	static constexpr uint8_t IoHeight() { return IoWidth() / 2; }

public:
	using Rssi = LayoutElement<X() + 1, Y() + 1, RssiWidth(), RssiHeight()>;

	using QualityUp = LayoutElement <Rssi::X() + Rssi::Width() + 2, Y() + 5, QualityWidth(), QualityHeight() >;
	using QualityDown = LayoutElement<QualityUp::X() + QualityWidth() + 1, Y() + 5, QualityWidth(), QualityHeight()>;
	using QualityClock = LayoutElement<QualityDown::X() + QualityWidth() + 1, Y() + 5, QualityWidth(), QualityHeight()>;
	using QualityAge = LayoutElement<X(), Rssi::Y() + Rssi::Height() + 1, Width(), QualityHeight()>;

	using IoUp = LayoutElement<Rssi::X() + 1, Rssi::Y() + Rssi::Height() - RssiPoleHeight(), IoWidth(), IoHeight()>;
	using IoDown = LayoutElement<Rssi::X() + Rssi::Width() - IoWidth() - 2, Rssi::Y() + Rssi::Height() - RssiPoleHeight(), IoWidth(), IoHeight()>;

	using Clock = ClockLayoutElement<X(), QualityAge::Y() + 2, Width(), height - QualityAge::Y() - QualityAge::Height()>;
};
#endif