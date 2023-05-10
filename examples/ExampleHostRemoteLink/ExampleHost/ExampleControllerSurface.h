// ControllerSurface.h

#ifndef _CONTROLLER_SURFACE_h
#define _CONTROLLER_SURFACE_h

#include <BitTracker.h>
#include <Services\SyncSurface\ITrackedSurface.h>


#define CONTROLLER_SURFACE_BLOCK_COUNT 2

class ExampleControllerSurface : public TemplateTrackedSurface<CONTROLLER_SURFACE_BLOCK_COUNT>
{
public:
	static const uint8_t Port = 50;

private:
	uint16_t LastDirection = 0;
	uint16_t LastPropulsion = 0;

public:
	ExampleControllerSurface()
		: TemplateTrackedSurface<CONTROLLER_SURFACE_BLOCK_COUNT>()
	{
	}

	////Fast changing values, we cache their last value to avoid resending the same stuff.
	//int16_t GetDirection()
	//{
	//	return Get16(CONTROLLER_DIRECTION_ADDRESS);
	//}
	//uint16_t GetPropulsion()
	//{
	//	return Get16(CONTROLLER_PROPULSION_ADDRESS);
	//}
	//void SetDirection(const uint16_t value)
	//{
	//	if (LastDirection != value)
	//	{
	//		LastDirection = value;
	//		Set16(value, CONTROLLER_DIRECTION_ADDRESS);
	//	}
	//}
	//void SetPropulsion(const uint16_t value)
	//{
	//	if (LastPropulsion != value)
	//	{
	//		LastPropulsion = value;
	//		Set16(value, CONTROLLER_PROPULSION_ADDRESS);
	//	}
	//}

	//int16_t GetDirectionTrim()
	//{
	//	return Get16(CONTROLLER_DIRECTION_TRIM_ADDRESS);
	//}
	//int16_t GetPropulsionTrim()
	//{
	//	return Get16(CONTROLLER_PROPULSION_TRIM_ADDRESS);
	//}
	//void SetDirectionTrim(const int16_t value)
	//{
	//	Set16(value, CONTROLLER_DIRECTION_TRIM_ADDRESS);
	//}
	//void SetPropulsionTrim(const int16_t value)
	//{
	//	Set16(value, CONTROLLER_PROPULSION_TRIM_ADDRESS);
	//}
};
#endif