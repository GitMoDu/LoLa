// LinkGraphicsEngine.h

#ifndef _LINK_GRAPHICS_ENGINE_h
#define _LINK_GRAPHICS_ENGINE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>

#include "ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>
#include "LinkDisplayDrawer.h"
#include "ChannelDisplayDrawer.h"

template<typename screenDriverClass, typename frameBufferType>
class LinkGraphicsEngine
{
private:
	const uint32_t FramePeriod = 16666;


private:
	screenDriverClass ScreenDriver;
	frameBufferType FrameBuffer;

private:
	uint8_t Buffer[frameBufferType::BufferSize]{};

	GraphicsEngineTask GraphicsEngine{};

	MultiDrawerWrapper<2> MultiDrawer;

	LinkDisplayDrawer LinkDrawer;
	ChannelDisplayDrawer ChannelDrawer;

#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
	EngineLogTask<1000> EngineLog;
#endif


public:
	LinkGraphicsEngine(Scheduler* scheduler, ILoLaLink* link, ILoLaTransceiver* transceiver = nullptr)
		: ScreenDriver()
		, FrameBuffer(Buffer)
		, GraphicsEngine(scheduler, &FrameBuffer, &ScreenDriver, FramePeriod)
		, LinkDrawer(&FrameBuffer, link, 0, 0, frameBufferType::FrameWidth, (frameBufferType::FrameHeight / 2) - 1)
		, ChannelDrawer(&FrameBuffer, transceiver, 1, (frameBufferType::FrameHeight / 2) + 1, frameBufferType::FrameWidth - 1, (frameBufferType::FrameHeight / 2) - 1)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, &GraphicsEngine)
#endif
	{}

	const bool Start()
	{
		if (!MultiDrawer.AddDrawer(&LinkDrawer)
			|| !MultiDrawer.AddDrawer(&ChannelDrawer)
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

