// ControllerExampleSurfaceDispatcher.h

#ifndef _CONTROLER_EXAMPLE_SURFACE_DISPATCHER_h
#define _CONTROLER_EXAMPLE_SURFACE_DISPATCHER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <IInputController.h>
#include <ILoLaInclude.h>

#include "ExampleSurface.h"

class ControllerExampleSurfaceDispatcher final : public virtual IDispatcher
{
private:
	ExampleSurface* Surface;
	IInputController* Controller;

public:
	ControllerExampleSurfaceDispatcher(Scheduler& scheduler
		, IInputController* controller
		, ExampleSurface* surface)
		: IDispatcher()
		, Surface(surface)
		, Controller(controller)
	{}

	void Start()
	{
		Controller->SetDispatcher(this);
	}

	virtual void OnDataUpdated() final
	{
#if defined (USE_GAMECUBE_CONTROLLER)
		const uint16_t steering = Controller->GetJoy1X();
		const uint16_t accelerator = Controller->GetTriggerR();
		const uint16_t brake = Controller->GetTriggerL();

		Surface->SetSteering(steering);
		Surface->SetAccelerator(accelerator);
		Surface->SetBrake(brake);
#else
		const uint16_t steering = Controller->GetJoy1X();
		const uint16_t accelerator = Controller->GetJoy1Y();

		Surface->SetSteering(steering);
		Surface->SetAccelerator(accelerator);
#endif
	}

	virtual void OnStateChanged(const bool connected)
	{
		Surface->SetBrake(!connected * UINT16_MAX);
	}
};
#endif