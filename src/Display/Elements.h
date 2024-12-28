// Elements.h

#ifndef _LINK_DISPLAY_ELEMENTS_h
#define _LINK_DISPLAY_ELEMENTS_h

#include <ArduinoGraphicsDrawer.h>
#include <ILoLaInclude.h>

#include <IntegerSignal.h>

namespace LoLa::Display
{
	namespace Elements
	{
		namespace LinkUpdate
		{
			struct IListener
			{
				virtual void OnLinkedUpdate(LoLaLinkExtendedStatus& linkStatus) {}
				virtual void OnUnlinkedUpdate() {}
			};

			/// <summary>
			/// Updates Link status every frame.
			/// </summary>
			class Drawer : public IFrameDraw
			{
			public:

			private:
				LoLaLinkExtendedStatus LinkStatus{};

				IntegerSignal::Filters::DemaU8<3> AgeFilter{};

			private:
				ILoLaLink& Link;

				IListener& Listener;

			private:
				bool Alive = false;

			public:
				Drawer(ILoLaLink& link, IListener& listener)
					: IFrameDraw()
					, Link(link)
					, Listener(listener)
				{
				}

				virtual const bool DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter)
				{
					if (Link.HasLink())
					{
						Link.GetLinkStatus(LinkStatus);
						Listener.OnLinkedUpdate(LinkStatus);

						if (Alive)
						{
							LinkStatus.OnUpdate();
						}
						else
						{
							LinkStatus.OnStart();
						}
						Alive = true;
					}
					else
					{
						Alive = false;
						Listener.OnUnlinkedUpdate();
					}

					return true;
				}
			};
		}

		namespace LinkClock
		{
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

				static constexpr uint8_t WidthRatio() { return 210; }

				static constexpr uint8_t KerningsWidth() { return FontKerning() * KerningCount(); }

				static constexpr uint8_t FontWidth()
				{
					return Smallest(FontStyle::GetFontWidth(WidthRatio(), height), (CharsPlusKerningsWidth() - KerningsWidth() - 4) / CharCount());
				}

				static constexpr uint8_t FontHeight()
				{
					return Smallest(height, FontStyle::GetFontHeight(WidthRatio(), FontWidth()));
				}

				static constexpr uint8_t EffectiveWidth() { return ((FontWidth() + FontKerning() + FontWidth()) * 3) + ((SpacerWidth()) * 2); }
				static constexpr uint8_t MarginStart() { return (width - EffectiveWidth()) / 2; }

			private:
				static constexpr uint8_t Smallest(const uint8_t a, const uint8_t b) { return ((b >= a) * a) | ((b < a) * b); }
			};

			template<typename Layout>
			class Drawer : public ElementDrawer
			{
			private:
				struct DrawerLayout
				{
					static constexpr uint8_t FontWidth() { return ClockLayoutCalculator<Layout::Width(), Layout::Height()>::FontWidth(); }
					static constexpr uint8_t FontHeight() { return ClockLayoutCalculator<Layout::Width(), Layout::Height()>::FontHeight(); }
					static constexpr uint8_t FontKerning() { return ClockLayoutCalculator<Layout::Width(), Layout::Height()>::FontKerning(); }

					static constexpr uint8_t DigitHours1X()
					{
						return Layout::X() + ClockLayoutCalculator<Layout::Width(), Layout::Height()>::MarginStart();
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
						return Separator1X() + (ClockLayoutCalculator<Layout::Width(), Layout::Height()>::SpacerMargin() * 2);
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
						return Separator2X() + (ClockLayoutCalculator<Layout::Width(), Layout::Height()>::SpacerMargin() * 2);
					}

					static constexpr uint8_t DigitSeconds2X()
					{
						return DigitSeconds1X() + FontWidth() + FontKerning();
					}
				};

			private:
				enum class DrawElementsEnum : uint8_t
				{
					Clock1,
					Clock2,
					Clock3,
					Clock4,
					Clock5,
					Clock6,
					Clock7,
					EnumCount
				};

			private:
				FontStyle ClockFont{};

			private:
				uint8_t Hours = 0;
				uint8_t Minutes = 0;
				uint8_t Seconds = 0;

			public:
				Drawer() : ElementDrawer((uint8_t)DrawElementsEnum::EnumCount)
				{
					ClockFont.Height = DrawerLayout::FontHeight();
					ClockFont.Width = DrawerLayout::FontWidth();
					ClockFont.Kerning = DrawerLayout::FontKerning();
				}

				void SetColor(const RgbColor& color)
				{
					ClockFont.Color.SetFrom(color);
				}

				void Set(const uint32_t linkDurationSeconds)
				{
					if (!IsEnabled())
					{
						SetEnabled(true);
					}

					const uint32_t durationMinutes = linkDurationSeconds / 60;
					const uint32_t durationHours = (durationMinutes / 60);

					Hours = durationHours % 24;
					Minutes = durationMinutes % 60;
					Seconds = linkDurationSeconds % 60;
				}

				virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
				{
					switch ((DrawElementsEnum)elementIndex)
					{
					case DrawElementsEnum::Clock1:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitHours1X(), Layout::Y(), Hours / 10);
						break;
					case DrawElementsEnum::Clock2:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitHours2X(), Layout::Y(), Hours % 10);
						break;
					case DrawElementsEnum::Clock3:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitMinutes1X(), Layout::Y(), Minutes / 10);
						break;
					case DrawElementsEnum::Clock4:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitMinutes2X(), Layout::Y(), Minutes % 10);
						break;
					case DrawElementsEnum::Clock5:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitSeconds1X(), Layout::Y(), Seconds / 10);
						break;
					case DrawElementsEnum::Clock6:
						DigitRenderer::Digit(frame, ClockFont, DrawerLayout::DigitSeconds2X(), Layout::Y(), Seconds % 10);
						break;
					case DrawElementsEnum::Clock7:
						frame->Pixel(ClockFont.Color, DrawerLayout::Separator1X(), Layout::Y() + ((DrawerLayout::FontHeight() * 1) / 4) + 1);
						frame->Pixel(ClockFont.Color, DrawerLayout::Separator1X(), Layout::Y() + ((DrawerLayout::FontHeight() * 3) / 4) - 1);
						frame->Pixel(ClockFont.Color, DrawerLayout::Separator2X(), Layout::Y() + ((DrawerLayout::FontHeight() * 1) / 4) + 1);
						frame->Pixel(ClockFont.Color, DrawerLayout::Separator2X(), Layout::Y() + ((DrawerLayout::FontHeight() * 3) / 4) - 1);
						break;
					default:
						break;
					}
				}
			};
		}

		namespace LinkIndicator
		{
			static constexpr uint8_t Width = 11;
			static constexpr uint8_t Height = 9;

			namespace RssiLines
			{
				static constexpr uint8_t Line0Width = 5;
				static constexpr uint8_t Line1Width = 9;
				static constexpr uint8_t Line2Width = 11;

				template<typename Layout>
				struct RssiLayout
				{
					using Ball = LayoutElement<Layout::X() + 4, Layout::Y() + Layout::Height() - 3, 3, 3>;
					using Line0 = LayoutElement<Layout::X() + 3, Layout::Y() + 4, Width - 6, 2>;
					using Line1 = LayoutElement<Layout::X() + 1, Layout::Y() + 2, Width - 2, 3>;
					using Line2 = LayoutElement<Layout::X(), Layout::Y(), Width, 3>;
				};

				static void DrawBall(IFrameBuffer* frame, const RgbColor& color, const uint8_t x, const uint8_t y)
				{
					frame->Pixel(color, x + 1, y);
					frame->Pixel(color, x + 0, y + 1);
					frame->Pixel(color, x + 1, y + 1);
					frame->Pixel(color, x + 2, y + 1);
					frame->Pixel(color, x + 1, y + 2);
				}

				static void DrawLine0(IFrameBuffer* frame, const RgbColor& color, const uint8_t x, const uint8_t y)
				{
					frame->Line(color, x + 1,
						y,
						x + Line0Width - 1,
						y);
					frame->Pixel(color, x, y + 1);
					frame->Pixel(color, x + Line0Width - 1, y + 1);
				}

				static void DrawLine1(IFrameBuffer* frame, const RgbColor& color, const uint8_t x, const uint8_t y)
				{
					frame->Line(color, x + 2,
						y,
						x + Line1Width - 2,
						y);
					frame->Pixel(color, x + 1, y + 1);
					frame->Pixel(color, x + Line1Width - 2, y + 1);
					frame->Pixel(color, x, y + 2);
					frame->Pixel(color, x + Line1Width - 1, y + 2);
				}

				static void DrawLine2(IFrameBuffer* frame, const RgbColor& color, const uint8_t x, const uint8_t y)
				{
					frame->Line(color, x + 2,
						y,
						x + Line2Width - 2,
						y);
					frame->Pixel(color, x + 1, y + 1);
					frame->Pixel(color, x + Line2Width - 2, y + 1);
					frame->Pixel(color, x, y + 2);
					frame->Pixel(color, x + Line2Width - 1, y + 2);
				}
			};

			template<typename Layout>
			class LinkOffDrawer : public ElementDrawer
			{
			private:
				using DrawerLayout = RssiLines::RssiLayout<Layout>;

			private:
				enum class DrawElementsEnum : uint8_t
				{
					Ball,
					Rssi0,
					Rssi1,
					Rssi2,
					Mask0,
					Mask1,
					Mask2,
					Mask3,
					Line,
					EnumCount
				};

			public:
				RgbColor Mask = 0x000000;
				RgbColor Background = 0x6A6A6A;
				RgbColor Foreground = 0xFFFFFF;

			public:
				LinkOffDrawer() : ElementDrawer((uint8_t)DrawElementsEnum::EnumCount) {}

				virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
				{
					switch ((DrawElementsEnum)elementIndex)
					{
					case DrawElementsEnum::Ball:
						LinkIndicator::RssiLines::DrawBall(frame, Background, DrawerLayout::Ball::X(), DrawerLayout::Ball::Y());
						break;
					case DrawElementsEnum::Rssi0:
						LinkIndicator::RssiLines::DrawLine0(frame, Background, DrawerLayout::Line0::X(), DrawerLayout::Line0::Y());
						break;
					case DrawElementsEnum::Rssi1:
						LinkIndicator::RssiLines::DrawLine1(frame, Background, DrawerLayout::Line1::X(), DrawerLayout::Line1::Y());
						break;
					case DrawElementsEnum::Rssi2:
						LinkIndicator::RssiLines::DrawLine2(frame, Background, DrawerLayout::Line2::X(), DrawerLayout::Line2::Y());
						break;
					case DrawElementsEnum::Mask0:
						frame->Line(Mask, Layout::X(), Layout::Y(), Layout::X() + Layout::Width() - 2, Layout::Y() + Layout::Height() - 1);
						break;
					case DrawElementsEnum::Mask1:
						frame->Line(Mask, Layout::X(), Layout::Y() + 1, Layout::X() + Layout::Width() - 3, Layout::Y() + Layout::Height() - 1);
						break;
					case DrawElementsEnum::Mask2:
						frame->Line(Mask, Layout::X() + 2, Layout::Y(), Layout::X() + Layout::Width(), Layout::Y() + Layout::Height() - 1);
						break;
					case DrawElementsEnum::Mask3:
						frame->Line(Mask, Layout::X() + 3, Layout::Y(), Layout::X() + Layout::Width(), Layout::Y() + Layout::Height() - 2);
						break;
					case DrawElementsEnum::Line:
						frame->Line(Foreground, Layout::X() + 1, Layout::Y(), Layout::X() + Layout::Width() - 1, Layout::Y() + Layout::Height() - 1);
						break;
					default:
						break;
					}
				}
			};

			template<typename Layout>
			class LinkLiveDrawer : public ElementDrawer
			{
			public:
				static constexpr uint32_t IoActiveDuration = 180000;

			private:
				struct DrawerLayout : public RssiLines::RssiLayout<Layout>
				{
					using IoRx = LayoutElement<Layout::X(), Layout::Y() + Layout::Height() - 2, 3, 2>;
					using IoTx = LayoutElement<Layout::X() + Layout::Width() - 3, Layout::Y() + Layout::Height() - 2, 3, 2>;
				};

			private:
				enum class DrawElementsEnum : uint8_t
				{
					Update,
					Ball,
					Rssi0,
					Rssi1,
					Rssi2,
					IoRx,
					IoTx,
					EnumCount
				};

			private:
				class IoTracker
				{
				private:
					uint32_t LastStep = 0;
					uint16_t LastCount = 0;

				public:
					void Clear()
					{
						LastCount = 0;
						LastStep = 0;
					}

					void Step(const uint32_t frameTime, const uint16_t count)
					{
						if (count != LastCount)
						{
							LastCount = count;
							LastStep = frameTime;
						}
					}

					const bool IsActive(const uint32_t frameTime) const
					{
						return (frameTime - LastStep) <= IoActiveDuration;
					}

					const uint8_t GetDecayProgress(const uint32_t frameTime) const
					{
						if (IsActive(frameTime))
						{
							return ProgressScaler::GetProgress<IoActiveDuration>(frameTime - LastStep) / UINT8_MAX;
						}
						else
						{
							return UINT8_MAX;
						}
					}
				};

				static constexpr uint8_t Rssi1_3 = UINT8_MAX / 4;
				static constexpr uint8_t Rssi2_3 = ((uint16_t)UINT8_MAX * 2) / 3;

			public:
				RgbColor Background = 0x000000;
				RgbColor Neutral = 0x6A6A6A;
				RgbColor Foreground = 0xFFFFFF;

				RgbColor RxForeground = 0x00FF00;
				RgbColor TxForeground = 0xFF0000;

				RgbColor AgeOk = 0x00FF00;
				RgbColor AgeNotOk = 0xFF0000;

			private:
				IntegerSignal::Filters::DemaU8<3> AgeFilter{};

				IoTracker UpTracker{};
				IoTracker DownTracker{};

				uint16_t RxCount = 0;
				uint16_t TxCount = 0;
				RgbColor UpColor = 0x000000;
				RgbColor DownColor = 0x000000;

				RgbColor Color{};
				uint8_t RssiIndex = 0;

			public:
				LinkLiveDrawer() : ElementDrawer((uint8_t)DrawElementsEnum::EnumCount) {}

				void Set(const uint8_t rssi, const uint8_t ageQuality, const uint16_t rxCount, const uint16_t txCount)
				{
					if (!IsEnabled())
					{
						SetEnabled(true);
						DownTracker.Clear();
						UpTracker.Clear();
					}

					AgeFilter.Set(ageQuality);

					if (rssi >= Rssi2_3)
					{
						RssiIndex = 2;
					}
					else if (rssi >= Rssi1_3)
					{
						RssiIndex = 1;
					}
					else
					{
						RssiIndex = 0;
					}

					RxCount = rxCount;
					TxCount = txCount;
				}

				virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
				{
					switch ((DrawElementsEnum)elementIndex)
					{
					case DrawElementsEnum::Update:
						AgeFilter.Step();
						DownTracker.Step(frameTime, RxCount);
						UpTracker.Step(frameTime, RxCount);
						RgbColorUtil::InterpolateRgb(Foreground, AgeNotOk, AgeOk, AgeFilter.Get());
						RgbColorUtil::InterpolateRgb(DownColor, RxForeground, Background, DownTracker.GetDecayProgress(frameTime));
						RgbColorUtil::InterpolateRgb(UpColor, TxForeground, Background, UpTracker.GetDecayProgress(frameTime));
						break;
					case DrawElementsEnum::Ball:
						LinkIndicator::RssiLines::DrawBall(frame, Foreground, DrawerLayout::Ball::X(), DrawerLayout::Ball::Y());
						break;
					case DrawElementsEnum::Rssi0:
						GetRssiColor(Color, frame, frameCounter, 0);
						LinkIndicator::RssiLines::DrawLine0(frame, Color, DrawerLayout::Line0::X(), DrawerLayout::Line0::Y());
						break;
					case DrawElementsEnum::Rssi1:
						GetRssiColor(Color, frame, frameCounter, 1);
						LinkIndicator::RssiLines::DrawLine1(frame, Color, DrawerLayout::Line1::X(), DrawerLayout::Line1::Y());
						break;
					case DrawElementsEnum::Rssi2:
						GetRssiColor(Color, frame, frameCounter, 2);
						LinkIndicator::RssiLines::DrawLine2(frame, Color, DrawerLayout::Line2::X(), DrawerLayout::Line2::Y());
						break;
					case DrawElementsEnum::IoRx:
						frame->Pixel(DownColor, DrawerLayout::IoRx::X() + 1, DrawerLayout::IoRx::Y() + 1);
						frame->Pixel(DownColor, DrawerLayout::IoRx::X(), DrawerLayout::IoRx::Y());
						frame->Pixel(DownColor, DrawerLayout::IoRx::X() + 1, DrawerLayout::IoRx::Y());
						frame->Pixel(DownColor, DrawerLayout::IoRx::X() + 2, DrawerLayout::IoRx::Y());
						break;
					case DrawElementsEnum::IoTx:
						frame->Pixel(UpColor, DrawerLayout::IoTx::X(), DrawerLayout::IoTx::Y() + 1);
						frame->Pixel(UpColor, DrawerLayout::IoTx::X() + 1, DrawerLayout::IoTx::Y() + 1);
						frame->Pixel(UpColor, DrawerLayout::IoTx::X() + 2, DrawerLayout::IoTx::Y() + 1);
						frame->Pixel(UpColor, DrawerLayout::IoTx::X() + 1, DrawerLayout::IoTx::Y());
						break;
					default:
						break;
					}
				}

				void GetRssiColor(RgbColor& color, IFrameBuffer* frame, const uint16_t frameCounter, const uint8_t index)
				{
					if (RssiIndex >= index)
					{
						color.SetFrom(Foreground);
					}
					else
					{
						// Fallback draw solution where color shading isn't available.
						if (frame->GetColorDepth() > 1)
						{
							color.SetFrom(Neutral);
						}
						else
						{
							if (frameCounter & 0b00000001)
							{
								color.FromRGB32(0xFFFFFF);
							}
							else
							{
								color.FromRGB32(0);
							}
						}
					}
				}
			};

			template<typename Layout>
			class LinkSearchingDrawer : public ElementDrawer
			{
			public:
				static constexpr uint32_t SearchDuration = 1000000;
				static constexpr uint8_t LinesCount = 2;

			private:
				using DrawerLayout = RssiLines::RssiLayout<Layout>;

				static constexpr uint8_t YRange = Layout::Height() - 3;

				enum class DrawElementsEnum : uint8_t
				{
					Ball,
					LineN,
					EnumCount = (uint8_t)LineN + LinesCount
				};

			public:
				RgbColor Background = 0x6A6A6A;
				RgbColor Foreground = 0xFFFFFF;
				RgbColor NotOk = 0xFF0000;

			public:
				LinkSearchingDrawer() : ElementDrawer((uint8_t)DrawElementsEnum::EnumCount) {}

				virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
				{
					if ((DrawElementsEnum)elementIndex == DrawElementsEnum::Ball)
					{
						RgbColor color{};

						const uint16_t progress = ProgressScaler::TriangleResponse(ProgressScaler::GetProgress<SearchDuration * 2>(frameTime));

						RgbColorUtil::InterpolateRgb(color, NotOk, Foreground, progress >> 8);
						LinkIndicator::RssiLines::DrawBall(frame, color, DrawerLayout::Ball::X(), DrawerLayout::Ball::Y());
					}
					else
					{
						DrawLineN(frame, elementIndex - (uint8_t)DrawElementsEnum::LineN, ProgressScaler::GetProgress<SearchDuration>(frameTime));
					}
				}

			private:
				void DrawLineN(IFrameBuffer* frame, const uint8_t index, const uint16_t timeProgress)
				{
					RgbColor color{};

					const uint16_t progress = timeProgress + (((uint32_t)index * UINT16_MAX) / LinesCount);

					RgbColorUtil::InterpolateRgbLinear(color, Foreground, Background, progress >> 8);

					const uint8_t offsetY = ProgressScaler::ScaleProgress(UINT16_MAX - progress, YRange);

					if (offsetY >= (((uint16_t)YRange * 2) / 3) - 1)
					{
						LinkIndicator::RssiLines::DrawLine0(frame, color, DrawerLayout::Line0::X(), Layout::Y() + offsetY);
					}
					else if (offsetY >= (((uint16_t)YRange * 1) / 3) - 1)
					{
						LinkIndicator::RssiLines::DrawLine1(frame, color, DrawerLayout::Line1::X(), Layout::Y() + offsetY);
					}
					else
					{
						LinkIndicator::RssiLines::DrawLine2(frame, color, DrawerLayout::Line2::X(), Layout::Y() + offsetY);
					}
				}
			};
		}

		namespace VerticalProgress
		{
			template<typename Layout>
			class Drawer : public ElementDrawer
			{
			public:
				RgbColor Outline = 0xFFFFFF;
				RgbColor Foreground = 0xFFFFFF;

			private:
				enum class DrawElementsEnum : uint8_t
				{
					Fill,
					Outline,
					EnumCount
				};

				uint8_t ProgressStart = 0;

			public:
				Drawer() : ElementDrawer((uint8_t)DrawElementsEnum::EnumCount) {}

				void Set(const uint8_t progress)
				{
					if (!IsEnabled())
					{
						SetEnabled(true);
					}

					const uint8_t progressHeight = ((uint16_t)(Layout::Height() - 2) * progress) / UINT8_MAX;

					ProgressStart = Layout::Y() + Layout::Height() - 2 - progressHeight;
				}

				virtual void DrawCall(IFrameBuffer* frame, const uint32_t frameTime, const uint16_t frameCounter, const uint8_t elementIndex) final
				{
					switch ((DrawElementsEnum)elementIndex)
					{
					case DrawElementsEnum::Fill:
						frame->RectangleFill(Foreground, Layout::X() + 1, ProgressStart, Layout::X() + Layout::Width() - 1, Layout::Y() + Layout::Height() - 1);
						break;
					case DrawElementsEnum::Outline:
						frame->Rectangle(Outline, Layout::X(), Layout::Y(), Layout::X() + Layout::Width(), Layout::Y() + Layout::Height());
						break;
					default:
						break;
					}
				}
			};
		}
	}
}
#endif