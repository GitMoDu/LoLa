// VirtualLinkGraphicsEngine.h

#ifndef _VIRTUAL_LINK_GRAPHICS_ENGINE_h
#define _VIRTUAL_LINK_GRAPHICS_ENGINE_h

#if defined(USE_LINK_DISPLAY)

#include <Wire.h>
#include <SPI.h>
#include <ILoLaInclude.h>

#include "../src/Testing/ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>

#include "Display/LinkDisplayDrawer.h"
#include "Display/ChannelDisplayDrawer.h"

template<typename frameBufferType>
class VirtualLinkGraphicsEngine
{
private:
	static constexpr uint32_t FramePeriod = 16666;

private:
	using LinkDisplayLeftLayout = LinkDisplayLayout<0, 0, (frameBufferType::FrameWidth / 2), (frameBufferType::FrameHeight / 2) - 1>;
	using LinkDisplayRightLayout = LinkDisplayLayout<frameBufferType::FrameWidth / 2, 0, frameBufferType::FrameWidth / 2, (frameBufferType::FrameHeight / 2) - 1>;
	using ChannelDisplayLeftLayout = LayoutElement<2, LinkDisplayLeftLayout::Height(), (frameBufferType::FrameWidth / 2) - 4, frameBufferType::FrameHeight - LinkDisplayLeftLayout::Height()>;
	using ChannelDisplayRightLayout = LayoutElement<(frameBufferType::FrameWidth / 2) + 2, LinkDisplayRightLayout::Height(), (frameBufferType::FrameWidth / 2) - 4, frameBufferType::FrameHeight - LinkDisplayRightLayout::Height()>;

	static constexpr uint8_t DrawerCount = 4;

private:
	MultiDrawerWrapper<DrawerCount> MultiDrawer{};

	uint8_t Buffer[frameBufferType::BufferSize]{};

private:
	frameBufferType FrameBuffer;

	GraphicsEngineTask GraphicsEngine;

	LinkDisplayDrawer<LinkDisplayLeftLayout> LinkDisplayLeft;
	LinkDisplayDrawer<LinkDisplayRightLayout> LinkDisplayRight;

	ChannelDisplayDrawer<ChannelDisplayLeftLayout> ChannelDisplayLeft;
	ChannelDisplayDrawer<ChannelDisplayRightLayout> ChannelDisplayRight;

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
		, ChannelDisplayLeft(transceiverLeft)
		, ChannelDisplayRight(transceiverRight)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, GraphicsEngine)
#endif
	{
	}

	const bool Start()
	{
		MultiDrawer.ClearDrawers();

		if (!MultiDrawer.AddDrawer(&LinkDisplayLeft)
			|| !MultiDrawer.AddDrawer(&LinkDisplayRight))
		{
			return false;
		}

		if (!MultiDrawer.AddDrawer(&ChannelDisplayLeft)
			|| !MultiDrawer.AddDrawer(&ChannelDisplayRight))
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