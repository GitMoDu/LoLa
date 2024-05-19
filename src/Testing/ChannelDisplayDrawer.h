// ChannelDisplayDrawer.h

#ifndef _CHANNEL_DISPLAY_DRAWER_h
#define _CHANNEL_DISPLAY_DRAWER_h

#include <ArduinoGraphicsDrawer.h>
#include "../LoLaTransceivers/ILoLaTransceiver.h"

class ChannelDisplayDrawer : public ElementDrawer
{
private:
	static constexpr uint32_t ChannelUpdatePeriod = 20000;

	enum class DrawElementsEnum : uint8_t
	{
		UpdateChannel,
		Channels1,
		Channels2,
		DrawElementsCount
	};

private:
	ILoLaTransceiver* Transceiver;
	uint8_t* Log;

private:
	RgbColor Color{};
	uint32_t LastUpdate = 0;
	uint8_t LogIndex = 0;

private:
	const uint8_t OffsetX;
	const uint8_t OffsetY;
	const uint8_t Width;
	const uint8_t Height;

public:
	ChannelDisplayDrawer(IFrameBuffer* frame
		, ILoLaTransceiver* transceiver
		, const uint8_t x, const uint8_t y
		, const uint8_t width, const uint8_t height)
		: ElementDrawer(frame, (uint8_t)DrawElementsEnum::DrawElementsCount)
		, Transceiver(transceiver)
		, Log(new uint8_t[height]{})
		, OffsetX(x)
		, OffsetY(y)
		, Width(width* (transceiver != nullptr))
		, Height(height* (transceiver != nullptr))
	{
		Color.r = 255;
		Color.g = 255;
		Color.b = 255;
	}

	virtual void DrawCall(DrawState* drawState) final
	{
		switch (drawState->ElementIndex)
		{
		case (uint8_t)DrawElementsEnum::UpdateChannel:
			StepChannel(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Channels1:
			DrawChannels1(drawState);
			break;
		case (uint8_t)DrawElementsEnum::Channels2:
			DrawChannels2(drawState);
			break;
		default:
			break;
		}
	}

private:
	void StepChannel(DrawState* drawState)
	{
		if (Transceiver != nullptr
			&& drawState->FrameTime - LastUpdate > ChannelUpdatePeriod)
		{
			LastUpdate = drawState->FrameTime;
			Log[LogIndex] = ((uint16_t)Transceiver->GetCurrentChannel() * (Width - 1)) / (Transceiver->GetChannelCount() - 1);
			LogIndex++;
			if (LogIndex >= Height)
			{
				LogIndex = 0;
			}
		}
	}

	void DrawChannels1(DrawState* drawState)
	{
		uint8_t index = 0;
		for (uint8_t i = 0; i < (Height / 2); i++)
		{
			index = (((uint16_t)LogIndex + Height - 1 - i) % Height);
			Frame->Pixel(Color, OffsetX + Log[index], OffsetY + i);
		}
	}

	void DrawChannels2(DrawState* drawState)
	{
		uint8_t index = 0;
		for (uint8_t i = (Height / 2); i < Height; i++)
		{
			index = (((uint16_t)LogIndex + Height - 1 - i) % Height);
			Frame->Pixel(Color, OffsetX + Log[index], OffsetY + i);
		}
	}
};
#endif