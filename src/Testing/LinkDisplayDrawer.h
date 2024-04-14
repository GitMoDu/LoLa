// LinkDisplayDrawer.h

#ifndef _LINK_DISPLAY_DRAWER_h
#define _LINK_DISPLAY_DRAWER_h

#include <ArduinoGraphicsDrawer.h>
#include <ILoLaInclude.h>

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

	static constexpr uint8_t RssiWidth = 23;
	static constexpr uint8_t RssiHeight = 13;
	static constexpr uint8_t QualityWidth = 3;
	static constexpr uint8_t ClockCharCount = 8;

	static constexpr uint8_t RssiLinesCount = 5;
	static constexpr uint8_t ClockSpacerWidth = 3;

	static constexpr uint32_t IoActiveDuration = 100000;
	static constexpr uint32_t IoFreshDuration = 15000;

	static constexpr uint32_t SearchDuration = 700;
	static constexpr uint8_t SearchStagger = 23;

	const char* EmptyTimeText = reinterpret_cast<const char*>("--");

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

	LoLaLinkExtendedStatus LinkStatus{};

	IoTracker UpTracker{};
	IoTracker DownTracker{};

	bool Alive = false;

private:
	FontStyle ClockFont{};
	RgbColor Color{};

private:
	ILoLaLink* Link;

	const uint8_t OffsetX;
	const uint8_t OffsetY;
	const uint8_t Width;
	const uint8_t Height;

public:
	LinkDisplayDrawer(IFrameBuffer* frame, ILoLaLink* link,
		const uint8_t x, const uint8_t y, const uint8_t width, const uint8_t height)
		: ElementDrawer(frame, (uint8_t)DrawElementsEnum::DrawElementsCount)
		, Link(link)
		, OffsetX(x)
		, OffsetY(y)
		, Width(width)
		, Height(height)
	{
		const uint8_t clockCharWidth = ((GetClockWidth() - (ClockSpacerWidth * 2)) / ClockCharCount);
		const uint8_t defaultHeight = FontStyle::GetHeight(TinyFont::WIDTH_RATIO, clockCharWidth);
		const uint8_t remainingHeight = Height - GetClockY() - 1;

		ClockFont.SetStyle(120, 120, 120, defaultHeight);
		if (remainingHeight < defaultHeight)
		{
			ClockFont.SetStyle(120, 120, 120, remainingHeight);
		}
	}

	const uint8_t GetDrawerHeight()
	{
		return GetClockY() - OffsetY + ClockFont.Height;
	}

	virtual void DrawCall(DrawState* drawState) final
	{
		switch (drawState->ElementIndex)
		{
		case (uint8_t)DrawElementsEnum::UpdateLinkInfo:
			StepStatus(Link, drawState);
			break;
		case (uint8_t)DrawElementsEnum::Rssi:
			DrawRssi(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Io:
			DrawIo(drawState);
			break;
		case (uint8_t)DrawElementsEnum::QualityUp:
			DrawQualityUp(drawState);
			break;
		case (uint8_t)DrawElementsEnum::QualityDown:
			DrawQualityDown(drawState);
			break;
		case (uint8_t)DrawElementsEnum::QualityClock:
			DrawQualityClock(drawState);
			break;
		case (uint8_t)DrawElementsEnum::QualityAge:
			DrawQualityAge(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Clock1:
			DrawClock1(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Clock2:
			DrawClock2(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Clock3:
			DrawClock3(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Clock4:
			DrawClock4(drawState);
			break;
		default:
			break;
		}
	}

private:
	void StepStatus(ILoLaLink* link, DrawState* drawState)
	{
		if (link->HasLink())
		{
			link->GetLinkStatus(LinkStatus);
			if (Alive)
			{
				LinkStatus.OnUpdate();
			}
			else
			{
				LinkStatus.OnStart();
			}
			DownTracker.Step(drawState->FrameTime, LinkStatus.RxCount);
			UpTracker.Step(drawState->FrameTime, LinkStatus.TxCount);
			Alive = true;
		}
		else
		{
			Alive = false;
		}
	}

	void DrawRssi(DrawState* drawState)
	{
		if (Alive)
		{
			const uint8_t x = GetRssiX();
			const uint8_t y = GetRssiY();

			const uint8_t width = GetRssiWidth();
			const uint8_t height = GetRssiHeight();
			const uint8_t poleHeight = GetRssiPoleHeight();

			const uint8_t linesHeight = height - poleHeight - 2;
			const uint8_t middleX = x + ((width) / 2) - 1;
			const uint8_t crossover = ((uint16_t)(UINT8_MAX - LinkStatus.Quality.RxRssi) * (RssiLinesCount + 1)) / UINT8_MAX;

			uint8_t lineY = 0;
			uint8_t lineMargin = 0;
			uint8_t lineWidth = 0;
			for (uint_fast8_t i = 0; i < RssiLinesCount; i++)
			{
				lineY = y + (i * linesHeight) / (RssiLinesCount - 1);
				lineWidth = 3 + (((uint16_t)(RssiLinesCount - 1 - i) * (width - 3)) / (RssiLinesCount));
				lineMargin = (width - 1 - lineWidth) / 2;

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
				Frame->Line(Color, x + lineMargin, lineY, x + lineMargin + lineWidth - 1, lineY);
			}

			Color.r = 110;
			Color.g = 110;
			Color.b = 110;
			Frame->Line(Color, middleX, y + height - poleHeight, middleX, y + height - 1);
		}
		else
		{
			DrawRssiSearching(drawState);
		}
	}

	void DrawRssiSearching(DrawState* drawState)
	{
		const uint8_t x = GetRssiX();
		const uint8_t y = GetRssiY();

		const uint8_t width = GetRssiWidth();
		const uint8_t height = GetRssiHeight();
		const uint8_t middleX = x + ((width) / 2) - 1;
		const uint8_t poleHeight = GetRssiPoleHeight();
		const uint8_t linesHeight = height - poleHeight - 2;

		uint8_t lineY = 0;
		uint8_t lineMargin = 0;
		uint8_t lineWidth = 0;
		for (uint_fast8_t i = 0; i < RssiLinesCount; i++)
		{
			lineY = y + (i * linesHeight) / (RssiLinesCount - 1);
			lineWidth = 3 + (((uint16_t)(RssiLinesCount - 1 - i) * (width - 3)) / (RssiLinesCount));
			lineMargin = (width - 1 - lineWidth) / 2;

			const uint32_t frameMillis = drawState->FrameTime / 1000;

			uint16_t progress;
			if (drawState->GetProgress(SearchDuration) >= INT16_MAX)
			{
				progress = ProgressScaler::GetProgress<SearchDuration / 2>(frameMillis + (i * SearchStagger));
			}
			else
			{
				progress = ProgressScaler::GetProgress<SearchDuration / 2>(frameMillis - (i * SearchStagger));
			}

			if (progress < (UINT16_MAX / RssiLinesCount))
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
			Frame->Line(Color, x + lineMargin, lineY, x + lineMargin + lineWidth - 1, lineY);
		}

		Color.r = 160;
		Color.g = 0;
		Color.b = 35;
		Frame->Line(Color, middleX, y + height - poleHeight, middleX, y + height - 1);
	}

	void DrawIo(DrawState* drawState)
	{
		const uint8_t y = GetIoY();
		const uint8_t width = GetIoWidth();
		const uint8_t height = GetIoHeight();

		if (UpTracker.IsActive(drawState->FrameTime))
		{
			if (UpTracker.IsFresh(drawState->FrameTime))
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

			const uint8_t x = GetIoUpX();
			const uint8_t middleX = x + ((width) / 2) - 1;
			for (uint_fast8_t i = 0; i < height; i++)
			{
				Frame->Line(Color, middleX - i, y + i, middleX + i, y + i);
			}
		}

		if (DownTracker.IsActive(drawState->FrameTime))
		{
			if (DownTracker.IsFresh(drawState->FrameTime))
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

			const uint8_t x = GetIoDownX();
			const uint8_t middleX = x + ((width) / 2);
			for (uint_fast8_t i = 0; i < height; i++)
			{
				Frame->Line(Color, middleX - i, y + (height - 1 - i), middleX + i, y + (height - 1 - i));
			}
		}
	}

	void DrawQualityUp(DrawState* drawState)
	{
		if (Alive)
		{
			const uint8_t x = GetQualityUpX();
			const uint8_t y = GetQualityY();
			const uint8_t width = GetQualityWidth();
			const uint8_t height = GetQualityHeight();
			const uint8_t txHeight = (((uint16_t)LinkStatus.Quality.TxDrop) * (height - 2)) / UINT8_MAX;

			Color.r = 40;
			Color.g = 40;
			Color.b = 90;
			Frame->Rectangle(Color, x, y, x + width, y + height);

			Color.r = 127;
			Color.g = 60;
			Color.b = 127;
			Frame->Line(Color, x + 1, y + height - 1 - txHeight, x + 1, y + height - 2);
		}
	}

	void DrawQualityDown(DrawState* drawState)
	{
		if (Alive)
		{
			const uint8_t x = GetQualityDownX();
			const uint8_t y = GetQualityY();
			const uint8_t width = GetQualityWidth();
			const uint8_t height = GetQualityHeight();
			const uint8_t rxHeight = (((uint16_t)LinkStatus.Quality.RxDrop) * (height - 2)) / UINT8_MAX;

			Color.r = 40;
			Color.g = 90;
			Color.b = 40;
			Frame->Rectangle(Color, x, y, x + width, y + height);

			Color.r = 90;
			Color.g = 127;
			Color.b = 127;
			Frame->Line(Color, x + 1, y + height - 1 - rxHeight, x + 1, y + height - 2);
		}
	}

	void DrawQualityClock(DrawState* drawState)
	{
		if (Alive)
		{
			const uint8_t x = GetQualityClockX();
			const uint8_t y = GetQualityY();
			const uint8_t width = GetQualityWidth();
			const uint8_t height = GetQualityHeight();
			const uint8_t clockHeight = (((uint16_t)LinkStatus.Quality.ClockSync) * (height - 2)) / UINT8_MAX;

			Color.r = 90;
			Color.g = 90;
			Color.b = 20;
			Frame->Rectangle(Color, x, y, x + width, y + height);

			Color.r = 120;
			Color.g = 120;
			Color.b = 120;
			Frame->Line(Color, x + 1, y + height - 1 - clockHeight, x + 1, y + height - 2);
		}
	}

	void DrawQualityAge(DrawState* drawState)
	{
		if (Alive)
		{
			const uint8_t x = GetQualityAgeX();
			const uint8_t y = GetQualityY();
			const uint8_t height = GetQualityHeight();
			const uint8_t ageHeight = (((uint16_t)LinkStatus.Quality.Age) * (height - 1)) / UINT8_MAX;

			Color.r = 120;
			Color.g = 90;
			Color.b = 60;
			Frame->Line(Color, x, y + height - 1 - ageHeight, x, y + height - 1);
		}
	}

	void DrawClock1(DrawState* drawState)
	{
		if (Alive)
		{
			DrawClockAlive1(drawState);
		}
		else
		{
			DrawClockDead1(drawState);
		}
	}

	void DrawClock2(DrawState* drawState)
	{
		if (Alive)
		{
			DrawClockAlive2(drawState);
		}
		else
		{
			DrawClockDead2(drawState);
		}
	}

	void DrawClock3(DrawState* drawState)
	{
		if (Alive)
		{
			DrawClockAlive3(drawState);
		}
		else
		{
			DrawClockDead3(drawState);
		}
	}

	void DrawClock4(DrawState* drawState)
	{
		if (Alive)
		{
			DrawClockAlive4(drawState);
		}
		else
		{
			DrawClockDead4(drawState);
		}
	}

	void DrawClockAlive1(DrawState* drawState)
	{
		const uint32_t durationMinutes = LinkStatus.DurationSeconds / 60;
		const uint32_t durationHours = (durationMinutes / 60) % 24;
		const uint8_t tens = durationHours / 10;
		const uint8_t units = durationHours % 10;
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		NumberRenderer::NumberTopLeft(Frame, ClockFont, x, y, tens);
		NumberRenderer::NumberTopLeft(Frame, ClockFont, x + ClockFont.Width + ClockFont.Kerning, y, units);
	}

	void DrawClockAlive2(DrawState* drawState)
	{
		const uint32_t durationMinutes = (LinkStatus.DurationSeconds / 60) % 60;
		const uint8_t tens = durationMinutes / 10;
		const uint8_t units = durationMinutes % 10;

		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		NumberRenderer::NumberTopLeft(Frame, ClockFont, x + (ClockFont.Width * 2) + (ClockFont.Kerning * 1) + ClockSpacerWidth - 1, y, tens);
		NumberRenderer::NumberTopLeft(Frame, ClockFont, x + (ClockFont.Width * 3) + (ClockFont.Kerning * 2) + ClockSpacerWidth - 1, y, units);
	}

	void DrawClockAlive3(DrawState* drawState)
	{
		const uint32_t durationSeconds = LinkStatus.DurationSeconds % 60;
		const uint8_t tens = durationSeconds / 10;
		const uint8_t units = durationSeconds % 10;
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		NumberRenderer::NumberTopLeft(Frame, ClockFont, x + (ClockFont.Width * 4) + (ClockFont.Kerning * 2) + ((ClockSpacerWidth - 1) * 2), y, tens);
		NumberRenderer::NumberTopLeft(Frame, ClockFont, x + (ClockFont.Width * 5) + (ClockFont.Kerning * 3) + ((ClockSpacerWidth - 1) * 2), y, units);
	}

	void DrawClockAlive4(DrawState* drawState)
	{
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 2) + ClockFont.Kerning, y + (ClockFont.Height / 2) - 1);
		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 2) + ClockFont.Kerning, y + (ClockFont.Height / 2) + 1);

		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 4) + ClockSpacerWidth + (ClockFont.Kerning * 2) - 1, y + (ClockFont.Height / 2) - 1);
		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 4) + ClockSpacerWidth + (ClockFont.Kerning * 2) - 1, y + (ClockFont.Height / 2) + 1);
	}

	void DrawClockDead1(DrawState* drawState)
	{
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		TextRenderer::TextTopLeft(Frame, ClockFont, x, y, EmptyTimeText);
	}

	void DrawClockDead2(DrawState* drawState)
	{
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();
		TextRenderer::TextTopLeft(Frame, ClockFont, x + (ClockFont.Width * 2) + ClockFont.Kerning + ClockSpacerWidth - 1, y, EmptyTimeText);
	}

	void DrawClockDead3(DrawState* drawState)
	{
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();
		TextRenderer::TextTopLeft(Frame, ClockFont, x + (ClockFont.Width * 4) + (ClockFont.Kerning * 2) + ((ClockSpacerWidth - 1) * 2), y, EmptyTimeText);
	}

	void DrawClockDead4(DrawState* drawState)
	{
		const uint8_t x = GetClockX();
		const uint8_t y = GetClockY();

		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 2) + ClockFont.Kerning, y + (ClockFont.Height / 2) - 1);
		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 2) + ClockFont.Kerning, y + (ClockFont.Height / 2) + 1);
		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 4) + ClockSpacerWidth + (ClockFont.Kerning * 2) - 1, y + (ClockFont.Height / 2) - 1);
		Frame->Pixel(ClockFont.Color, x + (ClockFont.Width * 4) + ClockSpacerWidth + (ClockFont.Kerning * 2) - 1, y + (ClockFont.Height / 2) + 1);
	}

private:
	const uint8_t GetRssiX()
	{
		return OffsetX + 1;
	}

	const uint8_t GetRssiY()
	{
		return OffsetY + 1;
	}

	const uint8_t GetRssiWidth()
	{
		return RssiWidth;
	}

	const uint8_t GetRssiHeight()
	{
		return RssiHeight;
	}

	const uint8_t GetIoY()
	{
		return GetRssiY() + GetRssiHeight() - GetIoHeight();
	}

	const uint8_t GetIoUpX()
	{
		return GetRssiX() + 1;
	}

	const uint8_t GetIoDownX()
	{
		return GetRssiX() + GetRssiWidth() - GetIoWidth() - 2;
	}

	const uint8_t GetIoWidth()
	{
		return (((GetRssiWidth() - 1) / 2) - 2);
	}

	const uint8_t GetIoHeight()
	{
		return GetIoWidth() / 2;
	}

	const uint8_t GetQualityUpX()
	{
		return GetRssiX() + GetRssiWidth() + 2;
	}

	const uint8_t GetQualityDownX()
	{
		return GetQualityUpX() + GetQualityWidth() + 1;
	}

	const uint8_t GetQualityClockX()
	{
		return GetQualityDownX() + GetQualityWidth() + 1;
	}

	const uint8_t GetQualityAgeX()
	{
		return GetQualityClockX() + GetQualityWidth() + 1;
	}

	const uint8_t GetQualityY()
	{
		return OffsetY + 5;
	}

	const uint8_t GetQualityWidth()
	{
		return QualityWidth;
	}

	const uint8_t GetQualityHeight()
	{
		return GetRssiHeight() - 4;
	}

	const uint8_t GetClockY()
	{
		return GetRssiY() + GetRssiHeight() + 2;
	}

	const uint8_t GetClockX()
	{
		const uint8_t effectiveWidth = (ClockFont.Width * 6) + (ClockFont.Kerning * 3) + (2 * ClockSpacerWidth) - 6;
		const uint8_t marginStart = (Width - effectiveWidth) / 2;

		return OffsetX + marginStart;
	}

	const uint8_t GetClockWidth()
	{
		return Width;
	}

	const uint8_t GetRssiPoleHeight()
	{
		return GetRssiHeight() / (RssiLinesCount - 1);
	}
};
#endif