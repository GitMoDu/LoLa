// LinkGraphicsEngine.h

#ifndef _LINK_GRAPHICS_ENGINE_h
#define _LINK_GRAPHICS_ENGINE_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <ILoLaInclude.h>

#include "../Testing/ExampleDisplayDefinitions.h"

#include <ArduinoGraphicsEngineTask.h>
#include "LinkDisplayDrawer.h"
#include "ChannelDisplayDrawer.h"

template<typename frameBufferType>
class LinkGraphicsEngine
{
private:
	const uint32_t FramePeriod = 16665;

	using LinkLayout = LinkDisplayLayout<0, 0, frameBufferType::FrameWidth, frameBufferType::FrameHeight / 2>;
	using ChannelLayout = LayoutElement<1, LinkLayout::Height() + 1, frameBufferType::FrameWidth - 2, frameBufferType::FrameHeight - LinkLayout::Height() >;

	static constexpr uint8_t DrawerCount = 2;

private:
	MultiDrawerWrapper<DrawerCount> MultiDrawer{};
	uint8_t Buffer[frameBufferType::BufferSize]{};

private:
	frameBufferType FrameBuffer;
	GraphicsEngineTask GraphicsEngine;

private:
	LinkDisplayDrawer<LinkLayout> LinkDrawer;
	ChannelDisplayDrawer<ChannelLayout> ChannelDrawer;

#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
	EngineLogTask<1000> EngineLog;
#endif

public:
	LinkGraphicsEngine(TS::Scheduler& scheduler,
		IScreenDriver& screenDriver,
		ILoLaLink& link,
		ILoLaTransceiver& transceiver)
		: FrameBuffer(Buffer)
		, GraphicsEngine(&scheduler, &FrameBuffer, &screenDriver, FramePeriod)
		, LinkDrawer(link)
		, ChannelDrawer(transceiver)
#if defined(DEBUG) && (defined(GRAPHICS_ENGINE_DEBUG) || defined(GRAPHICS_ENGINE_MEASURE))
		, EngineLog(scheduler, GraphicsEngine)
#endif
	{
	}

	const bool Start()
	{
		MultiDrawer.ClearDrawers();

		if (!MultiDrawer.AddDrawer(&LinkDrawer)
			|| !MultiDrawer.AddDrawer(&ChannelDrawer))
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

