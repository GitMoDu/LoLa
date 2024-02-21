// ExampleSurface.h

#ifndef _EXAMPLE_SURFACE_h
#define _EXAMPLE_SURFACE_h

#include <ISurfaceInclude.h>

namespace ExampleSurfaceDefinition
{
	struct CommandStruct
	{
		uint8_t Command64Bits[ISurface::BytesPerBlock]{};
	};

	struct ControlStruct
	{
		uint16_t Steering = UINT16_MAX / 2;
		uint16_t Accelerator = 0;
		uint16_t Brake = 0;
		uint16_t PADDING = 0;
	};

	struct BlockDataStruct
	{
		CommandStruct Command{};
		ControlStruct Control{};
	};

	static constexpr uint8_t CommandIndex = offsetof(BlockDataStruct, Command) / ISurface::BytesPerBlock;
	static constexpr uint8_t ControlIndex = offsetof(BlockDataStruct, Control) / ISurface::BytesPerBlock;
	static constexpr uint8_t BlockCount = sizeof(BlockDataStruct) / ISurface::BytesPerBlock;
};

class ExampleSurface : public TemplateSurface<ExampleSurfaceDefinition::BlockCount, 2>
{
public:
	static constexpr uint8_t Port = 11;
	static constexpr uint32_t Id = 1800444444;
	static constexpr uint32_t UpdatePeriodMillis = 10;

private:
	using BaseClass = TemplateSurface<ExampleSurfaceDefinition::BlockCount, 2>;

	using BaseClass::OnBlockUpdated;

private:
	ExampleSurfaceDefinition::BlockDataStruct Surface{};

public:
	ExampleSurface()
		: BaseClass((uint8_t*)&Surface)
	{}

	const uint16_t GetSteering()
	{
		return Surface.Control.Steering;
	}

	void SetSteering(const uint16_t value, const bool notify = true)
	{
		Surface.Control.Steering = value;
		OnBlockUpdated(ExampleSurfaceDefinition::ControlIndex, notify);
	}

	const uint16_t GetAccelerator()
	{
		return Surface.Control.Accelerator;
	}

	void SetAccelerator(const uint16_t value, const bool notify = true)
	{
		Surface.Control.Accelerator = value;
		OnBlockUpdated(ExampleSurfaceDefinition::ControlIndex, notify);
	}

	const uint16_t GetBrake()
	{
		return Surface.Control.Brake;
	}

	void SetBrake(const uint16_t value, const bool notify = true)
	{
		Surface.Control.Brake = value;
		OnBlockUpdated(ExampleSurfaceDefinition::ControlIndex, notify);
	}
};
#endif