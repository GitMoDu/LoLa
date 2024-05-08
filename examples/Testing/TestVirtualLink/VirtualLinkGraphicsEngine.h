// VirtualLinkGraphicsEngine.h

#ifndef _VIRTUAL_LINK_GRAPHICS_ENGINE_h
#define _VIRTUAL_LINK_GRAPHICS_ENGINE_h

#if defined(USE_LINK_DISPLAY)

#include <ILoLaInclude.h>

#include "../src/Testing/ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>

#include "Testing/LinkDisplayDrawer.h"
#include "Testing/ChannelDisplayDrawer.h"

template<typename screenDriverType, typename frameBufferType>
class VirtualLinkGraphicsEngine
{
private:
	const uint32_t FramePeriod = 25000;

private:
	uint8_t Buffer[frameBufferType::BufferSize]{};

	screenDriverType ScreenDriver;
	frameBufferType FrameBuffer;

private:
	GraphicsEngineTask GraphicsEngine;

	MultiDrawerWrapper<4> MultiDrawer;

	LinkDisplayDrawer LinkDisplayLeft;
	LinkDisplayDrawer LinkDisplayRight;
	ChannelDisplayDrawer ChannelDisplayLeft;
	ChannelDisplayDrawer ChannelDisplayRight;

#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
	EngineLogTask<1000> EngineLog;
#endif

public:
	VirtualLinkGraphicsEngine(Scheduler* scheduler
		, ILoLaLink* linkLeft, ILoLaLink* linkRight
		, ILoLaTransceiver* transceiverLeft = nullptr, ILoLaTransceiver* transceiverRight = nullptr)
		: ScreenDriver()
		, FrameBuffer(Buffer)
		, GraphicsEngine(scheduler, &FrameBuffer, &ScreenDriver, FramePeriod)
		, MultiDrawer()
		, LinkDisplayLeft(&FrameBuffer, linkLeft, 0, 0, (frameBufferType::FrameWidth / 2), frameBufferType::FrameHeight / 2)
		, LinkDisplayRight(&FrameBuffer, linkRight, (frameBufferType::FrameWidth / 2), 0, (frameBufferType::FrameWidth / 2), frameBufferType::FrameHeight / 2)
		, ChannelDisplayLeft(&FrameBuffer, transceiverLeft, 1, LinkDisplayLeft.GetDrawerHeight() + 1, (frameBufferType::FrameWidth / 2) - 3, frameBufferType::FrameHeight - LinkDisplayLeft.GetDrawerHeight() - 1)
		, ChannelDisplayRight(&FrameBuffer, transceiverRight, (frameBufferType::FrameWidth / 2) + 1, LinkDisplayLeft.GetDrawerHeight() + 1, (frameBufferType::FrameWidth / 2) - 3, frameBufferType::FrameHeight - LinkDisplayLeft.GetDrawerHeight() - 1)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, &GraphicsEngine)
#endif
	{}

	const bool Start()
	{
		if (!MultiDrawer.AddDrawer(&LinkDisplayLeft)
			|| !MultiDrawer.AddDrawer(&LinkDisplayRight)
			)
		{
			return false;
		}

		if (!MultiDrawer.AddDrawer(&ChannelDisplayLeft)
			|| !MultiDrawer.AddDrawer(&ChannelDisplayRight)
			)
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