// LinkDisplayDrawer.h

#ifndef _LINK_DISPLAY_DRAWER_h
#define _LINK_DISPLAY_DRAWER_h

#include "LinkDisplayLayout.h"
#include <ILoLaInclude.h>
#include <IntegerSignal.h>

template<typename Layout>
class LinkDisplayDrawer : public ElementDrawer
{
private:
	enum class DrawElementsEnum : uint8_t
	{
		UpdateLinkInfo,
		Rssi,
		Io,
		QualityUp,
		QualityDown,
		QualityClock,
		QualityAge,
		Clock1,
		Clock2,
		Clock3,
		Clock4,
		DrawElementsCount
	};

private:
	class IoTracker
	{
	private:
		uint32_t LastStep = 0;
		uint16_t LastCount = 0;

	public:
		void Step(const uint32_t frameTime, const uint16_t count)
		{
			if (count != LastCount)
			{
				LastCount = count;
				LastStep = frameTime;
			}
		}

		const bool IsActive(const uint32_t frameTime)
		{
			return (frameTime - LastStep) <= IoActiveDuration;
		}

		const bool IsFresh(const uint32_t frameTime)
		{
			return (frameTime - LastStep) <= IoFreshDuration;
		}
	};

private:
	static constexpr uint32_t IoActiveDuration = 100000;
	static constexpr uint32_t IoFreshDuration = 15000;

	static constexpr uint32_t SearchDuration = 700;
	static constexpr uint8_t SearchStagger = 23;

	static constexpr uint32_t TextColorPeriodMicros = 3000000;

	const char* EmptyTime = "-";

private:
	LoLaLinkExtendedStatus LinkStatus{};

	IntegerSignal::Filters::DemaU8<3> AgeFilter{};

	IoTracker UpTracker{};
	IoTracker DownTracker{};

	bool Alive = false;

private:
	FontStyle ClockFont{};
	RgbColor Color{};
	RgbColor AgeColor1{};
	RgbColor AgeColor2{};
	RgbColor AgeColor{};
	uint8_t AgeWidth = 0;
private:
	ILoLaLink& Link;

public:
	LinkDisplayDrawer(ILoLaLink& link)
		: ElementDrawer((uint8_t)DrawElementsEnum::DrawElementsCount)
		, Link(link)
	{
		ClockFont.Height = Layout::Clock::FontHeight();
		ClockFont.Width = Layout::Clock::FontWidth();
		ClockFont.Kerning = Layout::Clock::FontKerning();

		AgeColor1.FromRGB32(0xFCE11E);
		AgeColor2.FromRGB32(0x19FF00);
	}

	virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
	{
		switch (elementIndex)
		{
		case (uint8_t)DrawElementsEnum::UpdateLinkInfo:
			StepStatus(frameTime);
			break;
		case (uint8_t)DrawElementsEnum::Rssi:
			DrawRssi(frame, frameTime);
			break;
		case (uint8_t)DrawElementsEnum::Io:
			DrawIo(frame, frameTime);
			break;
		case (uint8_t)DrawElementsEnum::QualityUp:
			DrawQualityUp(frame);
			break;
		case (uint8_t)DrawElementsEnum::QualityDown:
			DrawQualityDown(frame);
			break;
		case (uint8_t)DrawElementsEnum::QualityClock:
			DrawQualityClock(frame);
			break;
		case (uint8_t)DrawElementsEnum::QualityAge:
			DrawQualityAge(frame);
			break;
		case (uint8_t)DrawElementsEnum::Clock1:
			DrawClock1(frame);
			break;
		case (uint8_t)DrawElementsEnum::Clock2:
			DrawClock2(frame);
			break;
		case (uint8_t)DrawElementsEnum::Clock3:
			DrawClock3(frame);
			break;
		case (uint8_t)DrawElementsEnum::Clock4:
			DrawClock4(frame);
			break;
		default:
			break;
		}
	}

private:
	void StepStatus(const uint32_t frameTime)
	{
		if (Link.HasLink())
		{
			Link.GetLinkStatus(LinkStatus);
			if (Alive)
			{
				LinkStatus.OnUpdate();
			}
			else
			{
				LinkStatus.OnStart();
			}
			DownTracker.Step(frameTime, LinkStatus.RxCount);
			UpTracker.Step(frameTime, LinkStatus.TxCount);

			AgeFilter.Set(LinkStatus.Quality.Age);
			AgeFilter.Step();


			AgeWidth = (((uint16_t)AgeFilter.Get()) * (uint8_t)Layout::QualityAge::Width()) / UINT8_MAX;

			RgbColorUtil::InterpolateRgbLinear(AgeColor, AgeColor1, AgeColor2, AgeFilter.Get());

			Alive = true;
		}
		else
		{
			AgeColor.SetFrom(AgeColor1);
			Alive = false;
		}
	}

	void DrawRssi(IFrameBuffer* frame, const uint32_t frameTime)
	{
		if (Alive)
		{
			static constexpr uint8_t middleX = Layout::Rssi::X() + ((Layout::Rssi::Width() / 2) - 1);
			const uint8_t crossover = ((uint16_t)(UINT8_MAX - LinkStatus.Quality.RxRssi) * (Layout::RssiLinesCount() + 1)) / UINT8_MAX;

			uint8_t lineY = 0;
			uint8_t lineMargin = 0;
			uint8_t lineWidth = 0;
			for (uint_fast8_t i = 0; i < Layout::RssiLinesCount(); i++)
			{
				lineY = Layout::Rssi::Y() + (i * 2);
				lineWidth = 3 + (((uint16_t)(Layout::RssiLinesCount() - 1 - i) * (Layout::Rssi::Width() - 3)) / (Layout::RssiLinesCount()));
				lineMargin = (Layout::Rssi::Width() - 1 - lineWidth) / 2;

				if (i >= crossover)
				{
					Color.r = 110;
					Color.g = 110;
					Color.b = 110;
				}
				else
				{
					Color.r = 30;
					Color.g = 30;
					Color.b = 30;
				}
				frame->Line(Color, Layout::Rssi::X() + lineMargin, lineY, Layout::Rssi::X() + lineMargin + lineWidth - 1, lineY);
			}

			Color.r = 110;
			Color.g = 110;
			Color.b = 110;
			frame->Line(Color, middleX, Layout::Rssi::Y() + Layout::Rssi::Height() - Layout::RssiPoleHeight(), middleX, Layout::Rssi::Y() + Layout::Rssi::Height() - 1);
		}
		else
		{
			DrawRssiSearching(frame, frameTime);
		}
	}

	void DrawRssiSearching(IFrameBuffer* frame, const uint32_t frameTime)
	{
		static constexpr uint8_t middleX = Layout::Rssi::X() + ((Layout::Rssi::Width()) / 2);
		const uint8_t linesHeight = Layout::Rssi::Height() - Layout::RssiPoleHeight() - 2;

		uint8_t lineY = 0;
		uint8_t lineMargin = 0;
		uint8_t lineWidth = 0;
		for (uint_fast8_t i = 0; i < Layout::RssiLinesCount(); i++)
		{
			lineY = Layout::Rssi::Y() + (i * linesHeight) / (Layout::RssiLinesCount() - 1);
			lineWidth = 3 + (((uint16_t)(Layout::RssiLinesCount() - 1 - (lineY - Layout::Rssi::Y())) * (Layout::Rssi::Width() - 3)) / (Layout::RssiLinesCount()));
			lineMargin = (Layout::Rssi::Width() - 1 - lineWidth) / 2;

			const uint32_t frameMillis = frameTime / 1000;

			uint16_t progress;
			if (ProgressScaler::GetProgress<SearchDuration>(frameTime) >= INT16_MAX)
			{
				progress = ProgressScaler::GetProgress<SearchDuration / 2>(frameMillis + (i * SearchStagger));
			}
			else
			{
				progress = ProgressScaler::GetProgress<SearchDuration / 2>(frameMillis - (i * SearchStagger));
			}

			if (progress < (UINT16_MAX / Layout::RssiLinesCount()))
			{
				Color.r = 160;
				Color.g = 0;
				Color.b = 30;
			}
			else
			{
				Color.r = 30;
				Color.g = 30;
				Color.b = 30;
			}
			frame->Line(Color, Layout::Rssi::X() + lineMargin, lineY, Layout::Rssi::X() + lineMargin + lineWidth, lineY);
		}

		Color.r = 160;
		Color.g = 0;
		Color.b = 35;
		frame->Line(Color, middleX, Layout::Rssi::Y() + Layout::Rssi::Height() - Layout::RssiPoleHeight(), middleX, Layout::Rssi::Y() + Layout::Rssi::Height() - 1);
	}

	void DrawIo(IFrameBuffer* frame, const uint32_t frameTime)
	{
		if (UpTracker.IsActive(frameTime))
		{
			if (UpTracker.IsFresh(frameTime))
			{
				Color.r = 60;
				Color.g = 60;
				Color.b = 255;
			}
			else
			{
				Color.r = 60;
				Color.g = 60;
				Color.b = 130;
			}

			static constexpr uint8_t middleX = Layout::IoUp::X() + (Layout::IoUp::Width() / 2) - 1;
			for (uint_fast8_t i = 0; i < Layout::IoUp::Height(); i++)
			{
				frame->Line(Color, middleX - i, Layout::IoUp::Y() + i, middleX + i + 1, Layout::IoUp::Y() + i);
			}
		}

		if (DownTracker.IsActive(frameTime))
		{
			if (DownTracker.IsFresh(frameTime))
			{
				Color.r = 40;
				Color.g = 255;
				Color.b = 40;
			}
			else
			{
				Color.r = 60;
				Color.g = 90;
				Color.b = 60;
			}

			static constexpr uint8_t middleX = Layout::IoDown::X() + (Layout::IoDown::Width() / 2);
			for (uint_fast8_t i = 0; i < Layout::IoDown::Height(); i++)
			{
				frame->Line(Color, middleX - i, Layout::IoDown::Y() + (Layout::IoDown::Height() - 1 - i), middleX + i + 1, Layout::IoDown::Y() + (Layout::IoDown::Height() - 1 - i));
			}
		}
	}

	void DrawQualityUp(IFrameBuffer* frame)
	{
		if (Alive)
		{
			const uint8_t txHeight = (((uint16_t)LinkStatus.Quality.TxDrop) * (Layout::QualityUp::Height() - 2)) / UINT8_MAX;

			Color.r = INT8_MAX;
			Color.g = INT8_MAX;
			Color.b = INT8_MAX;
			frame->Rectangle(Color, Layout::QualityUp::X(), Layout::QualityUp::Y(), Layout::QualityUp::X() + Layout::QualityUp::Width(), Layout::QualityUp::Y() + Layout::QualityUp::Height());

			Color.r = 0;
			Color.g = 0;
			Color.b = UINT8_MAX;
			frame->Line(Color, Layout::QualityUp::X() + 1, Layout::QualityUp::Y() + Layout::QualityUp::Height() - 1 - txHeight, Layout::QualityUp::X() + 1, Layout::QualityUp::Y() + Layout::QualityUp::Height() - 1);
		}
	}

	void DrawQualityDown(IFrameBuffer* frame)
	{
		if (Alive)
		{
			const uint8_t rxHeight = (((uint16_t)LinkStatus.Quality.RxDrop) * (Layout::QualityDown::Height() - 2)) / UINT8_MAX;

			Color.r = INT8_MAX;
			Color.g = INT8_MAX;
			Color.b = INT8_MAX;
			frame->Rectangle(Color, Layout::QualityDown::X(), Layout::QualityDown::Y(), Layout::QualityDown::X() + Layout::QualityDown::Width(), Layout::QualityDown::Y() + Layout::QualityDown::Height());

			Color.r = 0;
			Color.g = UINT8_MAX;
			Color.b = 0;
			frame->Line(Color, Layout::QualityDown::X() + 1, Layout::QualityDown::Y() + Layout::QualityDown::Height() - 1 - rxHeight, Layout::QualityDown::X() + 1, Layout::QualityDown::Y() + Layout::QualityDown::Height() - 1);
		}
	}

	void DrawQualityClock(IFrameBuffer* frame)
	{
		if (Alive)
		{
			const uint8_t clockHeight = (((uint16_t)LinkStatus.Quality.ClockSync) * (Layout::QualityClock::Height() - 2)) / UINT8_MAX;

			Color.r = INT8_MAX;
			Color.g = INT8_MAX;
			Color.b = INT8_MAX;
			frame->Rectangle(Color, Layout::QualityClock::X(), Layout::QualityClock::Y(), Layout::QualityClock::X() + Layout::QualityClock::Width(), Layout::QualityClock::Y() + Layout::QualityClock::Height());

			Color.r = UINT8_MAX;
			Color.g = 0;
			Color.b = UINT8_MAX;
			frame->Line(Color, Layout::QualityClock::X() + 1, Layout::QualityClock::Y() + Layout::QualityClock::Height() - 1 - clockHeight, Layout::QualityClock::X() + 1, Layout::QualityClock::Y() + Layout::QualityClock::Height() - 1);
		}
	}

	void DrawQualityAge(IFrameBuffer* frame)
	{
		if (Alive)
		{
			frame->Line(AgeColor, Layout::QualityAge::X(), Layout::QualityAge::Y(), Layout::QualityAge::X() + AgeWidth, Layout::QualityAge::Y());
		}
	}

	void DrawClock1(IFrameBuffer* frame)
	{
		if (Alive)
		{
			ClockFont.Color.FromRGB32(0x727272);
			DrawClockAlive1(frame);
		}
		else
		{
			ClockFont.Color.FromRGB32(0x660008);
			DrawClockDead1(frame);
		}
	}

	void DrawClock2(IFrameBuffer* frame)
	{
		if (Alive)
		{
			DrawClockAlive2(frame);
		}
		else
		{
			DrawClockDead2(frame);
		}
	}

	void DrawClock3(IFrameBuffer* frame)
	{
		if (Alive)
		{
			DrawClockAlive3(frame);
		}
		else
		{
			DrawClockDead3(frame);
		}
	}

	void DrawClock4(IFrameBuffer* frame)
	{
		DrawClockSeparators(frame);
	}

	void DrawClockAlive1(IFrameBuffer* frame)
	{
		const uint32_t durationMinutes = LinkStatus.DurationSeconds / 60;
		const uint32_t durationHours = (durationMinutes / 60) % 24;
		const uint8_t tens = durationHours / 10;
		const uint8_t units = durationHours % 10;

		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitHours1X(), Layout::Clock::Y(), tens);
		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitHours2X(), Layout::Clock::Y(), units);
	}

	void DrawClockAlive2(IFrameBuffer* frame)
	{
		const uint32_t durationMinutes = (LinkStatus.DurationSeconds / 60) % 60;
		const uint8_t tens = durationMinutes / 10;
		const uint8_t units = durationMinutes % 10;

		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitMinutes1X(), Layout::Clock::Y(), tens);
		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitMinutes2X(), Layout::Clock::Y(), units);
	}

	void DrawClockAlive3(IFrameBuffer* frame)
	{
		const uint32_t durationSeconds = LinkStatus.DurationSeconds % 60;
		const uint8_t tens = durationSeconds / 10;
		const uint8_t units = durationSeconds % 10;

		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitSeconds1X(), Layout::Clock::Y(), tens);
		NumberRenderer::NumberTopLeft(frame, ClockFont, Layout::Clock::DigitSeconds2X(), Layout::Clock::Y(), units);
	}

	void DrawClockSeparators(IFrameBuffer* frame)
	{
		frame->Pixel(ClockFont.Color, Layout::Clock::Separator1X(), Layout::Clock::Y() + ((Layout::Clock::FontHeight() * 1) / 4) + 1);
		frame->Pixel(ClockFont.Color, Layout::Clock::Separator1X(), Layout::Clock::Y() + ((Layout::Clock::FontHeight() * 3) / 4) - 1);
		frame->Pixel(ClockFont.Color, Layout::Clock::Separator2X(), Layout::Clock::Y() + ((Layout::Clock::FontHeight() * 1) / 4) + 1);
		frame->Pixel(ClockFont.Color, Layout::Clock::Separator2X(), Layout::Clock::Y() + ((Layout::Clock::FontHeight() * 3) / 4) - 1);
	}

	void DrawClockDead1(IFrameBuffer* frame)
	{
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitHours1X(), Layout::Clock::Y(), EmptyTime);
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitHours2X(), Layout::Clock::Y(), EmptyTime);
	}

	void DrawClockDead2(IFrameBuffer* frame)
	{
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitMinutes1X(), Layout::Clock::Y(), EmptyTime);
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitMinutes2X(), Layout::Clock::Y(), EmptyTime);
	}

	void DrawClockDead3(IFrameBuffer* frame)
	{
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitSeconds1X(), Layout::Clock::Y(), EmptyTime);
		TextRenderer::TextTopLeft(frame, ClockFont, Layout::Clock::DigitSeconds2X(), Layout::Clock::Y(), EmptyTime);
	}
};
#endif