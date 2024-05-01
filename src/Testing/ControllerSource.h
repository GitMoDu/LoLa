// ControllerSource.h

#ifndef _CONTROLLERSOURCE_h
#define _CONTROLLERSOURCE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <IInputController.h>
#include <JoystickParser.h>


#if defined (USE_N64_CONTROLLER)
#include <SerialJoyBusN64Controller.h> // https://github.com/GitMoDu/NintendoControllerReader
#elif defined (USE_GAMECUBE_CONTROLLER)
#include <SerialJoyBusGCController.h> // https://github.com/GitMoDu/NintendoControllerReader
#else
#error No controller type specified.
#endif

/// <summary>
/// #define USE_N64_CONTROLLER
/// #define USE_GAMECUBE_CONTROLLER
/// </summary>
/// <typeparam name="PollPeriod"></typeparam>
template<const uint32_t PollPeriod>
class ControllerSource final : private Task, public virtual IInputController
{
private:
	static constexpr uint32_t PollDelay = 2;
	static constexpr uint32_t PollSleep = PollPeriod - PollDelay;

private:
#if defined (USE_N64_CONTROLLER)
	struct Calibration
	{
		//Joystick.
		static constexpr int8_t	JoyXMin = -74;
		static constexpr int8_t	JoyXMax = 67;
		static constexpr int8_t	JoyXOffset = 3;

		static constexpr int8_t	JoyYMin = -78;
		static constexpr int8_t	JoyYMax = 78;
		static constexpr int8_t	JoyYOffset = 0;

		static constexpr uint8_t JoyDeadZoneRadius = 2;
	};
#elif defined (USE_GAMECUBE_CONTROLLER)
	struct Calibration
	{
		// Joystick.
		static constexpr int8_t	JoyXMin = -108;
		static constexpr int8_t	JoyXMax = 95;
		static constexpr int8_t	JoyXOffset = 14;

		static constexpr int8_t	JoyYMin = -99;
		static constexpr int8_t	JoyYMax = 107;
		static constexpr int8_t	JoyYOffset = -1;

		static constexpr uint8_t JoyDeadZoneRadius = 16;// 16;

		// Joystick C.
		static constexpr int8_t	JoyCXMin = -88;
		static constexpr int8_t	JoyCXMax = 96;
		static constexpr int8_t	JoyCXOffset = 5;

		static constexpr int8_t	JoyCYMin = -92;
		static constexpr int8_t	JoyCYMax = 97;
		static constexpr int8_t	JoyCYOffset = -4;

		static constexpr uint8_t JoyCDeadZoneRadius = 4;

		//Trigers.
		static constexpr uint8_t TriggerLMin = 0;
		static constexpr uint8_t TriggerLMax = 180;
		static constexpr uint8_t TriggerLDeadZone = 45;

		static constexpr uint8_t TriggerRMin = 0;
		static constexpr uint8_t TriggerRMax = 180;
		static constexpr uint8_t TriggerRDeadZone = 40;
	};
#endif

private:
	enum class StateEnum
	{
		NoController,
		Polling,
		Reading
	};
private:
	// Common joystick parsers.
	JoystickCenteredAxis<int8_t, uint16_t, Calibration::JoyXMin, Calibration::JoyXMax, Calibration::JoyXOffset, Calibration::JoyDeadZoneRadius, 0, UINT16_MAX> AxisJoy1X;
	JoystickCenteredAxis<int8_t, uint16_t, Calibration::JoyYMin, Calibration::JoyYMax, Calibration::JoyYOffset, Calibration::JoyDeadZoneRadius, 0, UINT16_MAX> AxisJoy1Y;

#if defined (USE_N64_CONTROLLER)
	SerialJoyBusN64Controller JoyBusDevice;
#elif defined (USE_GAMECUBE_CONTROLLER)
	SerialJoyBusGCController JoyBusDevice;

	JoystickCenteredAxis<int8_t, uint16_t, Calibration::JoyCXMin, Calibration::JoyCXMax, Calibration::JoyCXOffset, Calibration::JoyCDeadZoneRadius, 0, UINT16_MAX> AxisJoy2X;
	JoystickCenteredAxis<int8_t, uint16_t, Calibration::JoyCYMin, Calibration::JoyCYMax, Calibration::JoyCYOffset, Calibration::JoyCDeadZoneRadius, 0, UINT16_MAX> AxisJoy2Y;

	JoystickLinearAxis<uint8_t, uint16_t, Calibration::TriggerLMin, Calibration::TriggerLMax, Calibration::TriggerLDeadZone, 0, UINT16_MAX > AxisTriggerL;
	JoystickLinearAxis<uint8_t, uint16_t, Calibration::TriggerRMin, Calibration::TriggerRMax, Calibration::TriggerRDeadZone, 0, UINT16_MAX> AxisTriggerR;
#endif


private:
	IDispatcher* Dispatcher = nullptr;

	uint32_t LastRead = 0;
	uint32_t LastPoll = 0;

	StateEnum State = StateEnum::NoController;

	bool GoodState = false;

public:
	ControllerSource(Scheduler& scheduler, HardwareSerial* serialInstance)
		: IInputController()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, JoyBusDevice(serialInstance)
	{}

	void Start()
	{
		if (PollPeriod >= PollDelay)
		{
			State = StateEnum::NoController;
			JoyBusDevice.Start();

			GoodState = false;
			Task::enable();
		}
	}

	void Stop()
	{
		JoyBusDevice.Stop();

		State = StateEnum::NoController;
		Task::disable();
	}

	virtual void SetDispatcher(IDispatcher* dispatcher)
	{
		Dispatcher = dispatcher;
	}

	virtual bool Callback() final
	{
		const uint32_t timestamp = millis();

		switch (State)
		{
		case StateEnum::NoController:
			if (GoodState)
			{
				GoodState = false;
				if (Dispatcher != nullptr)
				{
					Dispatcher->OnStateChanged(false);
				}
			}
			JoyBusDevice.Poll();
			LastRead = timestamp;
			Task::delay(10);
			State = StateEnum::Polling;
			break;
		case StateEnum::Polling:
			LastPoll = timestamp;
			State = StateEnum::Reading;
			Task::delay(PollDelay);

			// Poll can be sent and response read later, asynchronously.
			JoyBusDevice.Poll();
			break;
		case StateEnum::Reading:
			if (JoyBusDevice.Read())
			{
				if (!GoodState)
				{
					GoodState = true;
					if (Dispatcher != nullptr)
					{
						Dispatcher->OnStateChanged(true);
					}
				}
				else
				{
					if (Dispatcher != nullptr)
					{
						Dispatcher->OnDataUpdated();
					}
				}

				Task::delay(PollSleep);
				State = StateEnum::Polling;
				LastRead = timestamp;
			}
			else
			{
				if (timestamp - LastRead > 50)
				{
					State = StateEnum::NoController;
					Task::delay(50);
					return false;
				}
				else
				{
					Task::delay(50);
				}
			}
			break;
		default:
			break;
		}

		return true;
	}

	/// <summary>
	/// IInputController overrides.
	/// </summary>
public:
	virtual const uint16_t GetJoy1X() final
	{
		return AxisJoy1X.Parse(JoyBusDevice.Data.JoystickX);
	}

	virtual const uint16_t GetJoy1Y() final
	{
		return AxisJoy1X.Parse(JoyBusDevice.Data.JoystickY);
	}

	virtual const uint16_t GetJoy2X() final
	{
#if defined (USE_N64_CONTROLLER)
		if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CLeft>())
		{
			if (!JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CRight>()) {
				return INT16_MIN;
			}
		}
		else if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CRight>())
		{
			if (!JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CLeft>()) {
				return INT16_MAX;
			}
		}
		return 0;

#elif defined (USE_GAMECUBE_CONTROLLER)
		return AxisJoy2X.Parse(JoyBusDevice.Data.JoystickCX);
#endif
	}

	virtual const uint16_t GetJoy2Y() final
	{
#if defined (USE_N64_CONTROLLER)
		if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CDown>())
		{
			if (!JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CUp>()) {
				return INT16_MIN;
			}
		}
		else if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CUp>())
		{
			if (!JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::CDown>()) {
				return INT16_MAX;
			}
		}
		return 0;
#elif defined (USE_GAMECUBE_CONTROLLER)
		return AxisJoy2X.Parse(JoyBusDevice.Data.JoystickCY);
#endif
	}

	virtual const uint16_t GetTriggerL() final
	{
#if defined (USE_N64_CONTROLLER)
		if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::L>())
		{
			return UINT16_MAX;
		}
		else
		{
			return 0;
		}
#elif defined (USE_GAMECUBE_CONTROLLER)
		return AxisTriggerL.Parse(JoyBusDevice.Data.SliderLeft);
#endif
	}
	virtual const uint16_t GetTriggerR() final
	{
#if defined (USE_N64_CONTROLLER)
		if (JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::R>())
		{
			return UINT16_MAX;
		}
		else
		{
			return 0;
		}
#elif defined (USE_GAMECUBE_CONTROLLER)
		return AxisTriggerR.Parse(JoyBusDevice.Data.SliderRight);
#endif
	}

	virtual void GetDirection(bool& left, bool& right, bool& up, bool& down) final
	{
#if defined (USE_N64_CONTROLLER)
		left = JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Left>();
		right = JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Right>();
		up = JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Up>();
		down = JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Down>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		left = JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Left>();
		right = JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Right>();
		up = JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Up>();
		down = JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Down>();
#endif
	}

	virtual const bool GetLeft() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Left>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Left>();
#endif
	}
	virtual const bool GetRight() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Right>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Right>();
#endif
	}
	virtual const bool GetUp() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Up>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Up>();
#endif
	}
	virtual const bool GetDown() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Down>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Down>();
#endif
	}

	virtual const bool GetButton0() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::A>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::A>();
#endif
	}

	virtual const bool GetButton1() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::B>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::B>();
#endif
	}

	virtual const bool GetButton2() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Z>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::X>();
#endif
	}
	virtual const bool GetButton3()  final
	{
#if defined (USE_N64_CONTROLLER)
		return false;
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Y>();
#endif
	}
	virtual const bool GetButton4() final
	{
		return false;
	}
	virtual const bool GetButton5()  final
	{
		return false;
	}
	virtual const bool GetButton6()  final
	{
		return false;
	}
	virtual const bool GetButton7() final
	{
		return false;
	}
	virtual const bool GetButton8() final
	{
		return false;
	}
	virtual const bool GetButton9() final
	{
#if defined (USE_N64_CONTROLLER)
		return JoyBusDevice.Data.Button<Nintendo64::ButtonsEnum::Start>();
#elif defined (USE_GAMECUBE_CONTROLLER)
		return JoyBusDevice.Data.Button<GameCube::ButtonsEnum::Start>();
#endif
	}

	virtual const bool GetButtonAccept() final { return GetButton0(); }
	virtual const bool GetButtonReject() final { return GetButton1(); }
	virtual const bool GetButtonHome() final { return GetButton9(); }
};
#endif