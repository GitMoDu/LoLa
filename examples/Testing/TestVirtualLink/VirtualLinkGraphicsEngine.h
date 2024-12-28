// VirtualLinkGraphicsEngine.h

#ifndef _VIRTUAL_LINK_GRAPHICS_ENGINE_h
#define _VIRTUAL_LINK_GRAPHICS_ENGINE_h

#if defined(USE_LINK_DISPLAY)

#include <Wire.h>
#include <SPI.h>
#include <ILoLaInclude.h>

#include "../src/Testing/ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>

#include <LoLaDisplay.h>


template<typename frameBufferType>
class VirtualLinkGraphicsEngine
{
private:
	static constexpr uint32_t FramePeriod = 16666;

private:
	struct Layout
	{
		//// Mock Layout to calculate effective link display height.
		using LinkDisplay = LayoutElement<0, 0, (frameBufferType::FrameWidth / 2), frameBufferType::FrameHeight / 2>;

		// Font size aware display height: i.e. actual height.
		static constexpr uint32_t LinkDisplayHeight = LoLa::Display::LinkDebug::DisplayHeight<LinkDisplay>();

		using LinkDisplayLeft = LayoutElement<0, 0, (frameBufferType::FrameWidth / 2), LinkDisplayHeight>;
		using LinkDisplayRight = LayoutElement<frameBufferType::FrameWidth / 2, 0, frameBufferType::FrameWidth / 2, LinkDisplayHeight>;

		using ChannelDisplayLeft = LayoutElement<0, LinkDisplayLeft::Height() + 1, frameBufferType::FrameWidth, frameBufferType::FrameHeight - 2 - LinkDisplayLeft::Height()>;
	};

	static constexpr uint8_t DrawerCount = 3;

private:
	MultiDrawerWrapper<DrawerCount> MultiDrawer{};

	uint8_t Buffer[frameBufferType::BufferSize]{};

private:
	frameBufferType FrameBuffer;

	GraphicsEngineTask GraphicsEngine;

	LoLa::Display::LinkDebug::DrawerWrapper<typename Layout::LinkDisplayLeft> LinkDisplayLeft;
	LoLa::Display::LinkDebug::DrawerWrapper<typename Layout::LinkDisplayRight> LinkDisplayRight;

	LoLa::Display::ChannelHistory::Drawer<typename Layout::ChannelDisplayLeft> ChannelHistoryDisplay;

#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
	EngineLogTask<1000> EngineLog;
#endif

public:
	VirtualLinkGraphicsEngine(TS::Scheduler& scheduler,
		IScreenDriver& screenDriver,
		ILoLaLink& linkLeft, ILoLaLink& linkRight,
		ILoLaTransceiver& transceiverLeft,
		ILoLaTransceiver& transceiverRight)
		: FrameBuffer(Buffer)
		, GraphicsEngine(&scheduler, &FrameBuffer, &screenDriver, FramePeriod)
		, LinkDisplayLeft(linkLeft)
		, LinkDisplayRight(linkRight)
		, ChannelHistoryDisplay(transceiverLeft)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, GraphicsEngine)
#endif
	{
	}

	const bool Start()
	{
		MultiDrawer.ClearDrawers();

		if (!LinkDisplayLeft.AddDrawers()
			|| LinkDisplayRight.AddDrawers()
			|| !MultiDrawer.AddDrawer(&LinkDisplayLeft)
			|| !MultiDrawer.AddDrawer(&LinkDisplayRight)
			|| !MultiDrawer.AddDrawer(&ChannelHistoryDisplay))
		{
			return false;
		}

		GraphicsEngine.SetDrawer(&MultiDrawer);

		return GraphicsEngine.Start();
	}

	void SetBufferTaskCallback(void (*taskCallback)(void* parameter))
	{
		GraphicsEngine.SetBufferTaskCallback(taskCallback);
	}

	void BufferTaskCallback(void* parameter)
	{
		GraphicsEngine.BufferTaskCallback(parameter);
	}

	void SetInverted(const bool inverted)
	{
		GraphicsEngine.SetInverted(inverted);
	}
};
#endif
#endif