// LinkGraphicsEngine.h

#ifndef _LINK_GRAPHICS_ENGINE_h
#define _LINK_GRAPHICS_ENGINE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>

#include "ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>
#include "LinkDisplayDrawer.h"

template<typename screenDriverClass, typename frameBufferType>
class LinkGraphicsEngine
{
private:
	const uint32_t FramePeriod = 16666;

private:
	uint8_t Buffer[frameBufferType::BufferSize]{};

	screenDriverClass ScreenDriver;
	frameBufferType FrameBuffer;

private:
	GraphicsEngineTask GraphicsEngine;

	LinkDisplayDrawer LinkDrawer;

#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
	EngineLogTask<1000> EngineLog;
#endif

public:
	LinkGraphicsEngine(Scheduler* scheduler, ILoLaLink* link)
		: ScreenDriver()
		, FrameBuffer(Buffer)
		, GraphicsEngine(scheduler, &FrameBuffer, &ScreenDriver, FramePeriod)
		, LinkDrawer(&FrameBuffer, link, 0, 0, frameBufferType::FrameWidth, frameBufferType::FrameHeight)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, &GraphicsEngine)
#endif
	{}

	const bool Start()
	{
		GraphicsEngine.SetDrawer(&LinkDrawer);

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

