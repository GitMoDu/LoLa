// ChannelHistory.h

#ifndef _LOLA_DISPLAY_CHANNEL_HISTORY_h
#define _LOLA_DISPLAY_CHANNEL_HISTORY_h

#include <ArduinoGraphicsDrawer.h>
#include "../LoLaTransceivers/ILoLaTransceiver.h"

namespace LoLa::Display::ChannelHistory
{
	static constexpr uint32_t StepDuration = 20000;

	template<typename Layout>
	class Drawer : public ElementDrawer
	{
	private:
		enum class DrawElementsEnum : uint8_t
		{
			Update,
			ChannelsN,
			EnumCount = (uint8_t)ChannelsN + Layout::Height()
		};

	private:
		ILoLaTransceiver& Transceiver;

	private:
		// Sprite is used as a workaround to call BrightnessEffect.
		SpriteShaderEffect::BrightnessEffect<SpriteShader::ColorShader<RectangleSprite<1, 1>>> PixelSprite{};

		uint32_t LastStep = 0;
		uint8_t Log[Layout::Height()]{};
		uint8_t LogIndex = 0;

	public:
		RgbColor Foreground = 0xbebebe;

	public:
		Drawer(ILoLaTransceiver& transceiver)
			: ElementDrawer((uint8_t)DrawElementsEnum::EnumCount)
			, Transceiver(transceiver)
		{
			PixelSprite.SetColor(Foreground);
		}

		virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
		{
			if (Transceiver.GetChannelCount() > 1)
			{
				switch (elementIndex)
				{
				case (uint8_t)DrawElementsEnum::Update:
					if (frameTime - LastStep >= StepDuration)
					{
						LastStep = frameTime;
						StepChannel();
					}
					break;
				default:
					DrawChannelsLine(frame, elementIndex - (uint8_t)DrawElementsEnum::ChannelsN);
					break;
				}
			}
		}

	private:
		void StepChannel()
		{
			Log[LogIndex] = Layout::X() +
				(((uint16_t)Transceiver.GetCurrentChannel() * (Layout::Width() - 1)) / (Transceiver.GetChannelCount() - 1));
			LogIndex++;
			if (LogIndex >= Layout::Height())
			{
				LogIndex = 0;
			}
		}

		void DrawChannelsLine(IFrameBuffer* frame, const uint8_t line)
		{
			RgbColor color{};
			const uint8_t lineProgress = ((uint16_t)line * UINT8_MAX) / Layout::Height();
			const uint8_t curvedProgress = IntegerSignal::Curves::Root2U8<>::Get(lineProgress);
			const int8_t lineBrightness = ((int16_t)curvedProgress * INT8_MIN) / UINT8_MAX;

			PixelSprite.SetBrightness(lineBrightness);
			PixelSprite.Get(color, 0, 0);

			const uint8_t index = (((uint16_t)LogIndex + Layout::Height() - 1 - line) % Layout::Height());
			frame->Pixel(color, Layout::X() + Log[index], Layout::Y() + line);
		}
	};
}
#endif