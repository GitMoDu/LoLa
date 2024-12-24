// ChannelDisplayDrawer.h

#ifndef _CHANNEL_DISPLAY_DRAWER_h
#define _CHANNEL_DISPLAY_DRAWER_h

#include <ArduinoGraphicsDrawer.h>
#include "../LoLaTransceivers/ILoLaTransceiver.h"

template<typename Layout>
class ChannelDisplayDrawer : public ElementDrawer
{
private:
	enum class DrawElementsEnum : uint8_t
	{
		UpdateChannel,
		Channels1,
		Channels2,
		DrawElementsCount
	};

private:
	ILoLaTransceiver& Transceiver;

private:
	uint8_t Log[Layout::Height()]{};
	RgbColor Color{};
	RgbColor Color1{};
	RgbColor Color2{};
	uint8_t LogIndex = 0;

public:
	ChannelDisplayDrawer(ILoLaTransceiver& transceiver)
		: ElementDrawer((uint8_t)DrawElementsEnum::DrawElementsCount)
		, Transceiver(transceiver)
	{
		Color1.FromRGB32(0x332351);
		Color2.FromRGB32(0x6100FF);
	}

	virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
	{
		if (Transceiver.GetChannelCount() > 1)
		{
			switch (elementIndex)
			{
			case (uint8_t)DrawElementsEnum::UpdateChannel:
				StepChannel();
				break;
			case (uint8_t)DrawElementsEnum::Channels1:
				DrawChannels1(frame);
				break;
			case (uint8_t)DrawElementsEnum::Channels2:
				DrawChannels2(frame);
				break;
			default:
				break;
			}
		}
	}

private:
	void StepChannel()
	{
		Log[LogIndex] = ((uint16_t)Transceiver.GetCurrentChannel() * (Layout::Width() - 1)) / (Transceiver.GetChannelCount() - 1);
		LogIndex++;
		if (LogIndex >= Layout::Height())
		{
			LogIndex = 0;
		}
	}

	void DrawChannels1(IFrameBuffer* frame)
	{
		uint8_t index = 0;
		for (uint8_t i = 0; i < (Layout::Height() / 2); i++)
		{
			index = (((uint16_t)LogIndex + Layout::Height() - 1 - i) % Layout::Height());

			RgbColorUtil::InterpolateRgbLinear(Color, Color2, Color1, (((uint16_t)i * UINT8_MAX) / (Layout::Height() - 1)));

			frame->Pixel(Color, Layout::X() + Log[index], Layout::Y() + i);
		}
	}

	void DrawChannels2(IFrameBuffer* frame)
	{
		uint8_t index = 0;
		for (uint8_t i = (Layout::Height() / 2); i < Layout::Height(); i++)
		{
			index = (((uint16_t)LogIndex + Layout::Height() - 1 - i) % Layout::Height());
			RgbColorUtil::InterpolateRgbLinear(Color, Color2, Color1, (((uint16_t)i * UINT8_MAX) / (Layout::Height() - 1)));

			frame->Pixel(Color, Layout::X() + Log[index], Layout::Y() + i);
		}
	}
};
#endif