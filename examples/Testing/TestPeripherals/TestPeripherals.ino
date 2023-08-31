/* Test LoLa Peripheral Drivers.
*
*/


#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#endif

#define SERIAL_BAUD_RATE 115200

#define _TASK_OO_CALLBACKS
//#define _TASK_SLEEP_ON_IDLE_RUN
#include <TaskScheduler.h>

#include <ILoLaInclude.h>
#include <Arduino.h>

#include "TestTask.h"
#include "Tests.h"

// Process scheduler.
Scheduler SchedulerBase;
//

//static const uint32_t TIME_TEST_DELAY_MILLIS = 3124;

static constexpr uint32_t TIMER_CLOCK_TEST_SECONDS = 5;
static constexpr uint32_t TIMER_CLOCK_TEST_MAX_ERROR = 10;


ArduinoEntropy ArduinoEntropySource(123);
ArduinoTaskTimerClockSource<> ArduinoTimerClock(SchedulerBase);


#if defined(ARDUINO_ARCH_AVR)
#if defined(__AVR_ATmega328p__)
AvrTimer0TimerSource AvrTimer;
AvrTimer1ClockSource AvrClock;
#else
#error Avr Device not supported.
#endif
#endif


#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource Stm32Entropy;

static const uint8_t TimerStm32Index = 1;
static const uint8_t TimerStm32Channel = 'A';
Stm32TimerSource Stm32Timer(TimerStm32Index, TimerStm32Channel);
Stm32RtcClockSource Stm32Clock(SchedulerBase);
#endif

#if defined(ARDUINO_ESP8266)
#error ESP8266 TODO.
Esp8266EntropySource Esp8266Entropy;

static const uint8_t TimerEsp8266Index = 1;
//Esp8266TimerSource Esp8266Timer(TimerEsp8266Index);
//Esp8266ClockSource Esp8266Clock(SchedulerBase);
#endif

#if defined(ARDUINO_ESP32)
#error ESP32 TODO.
Esp32EntropySource Esp32Entropy;
static const uint8_t TimerEsp32Index = 1;
//Esp8266TimerSource Esp32Timer(TimerEsp8266Index);
//Esp8266ClockSource Esp32Clock(SchedulerBase);
#endif

TestTask<TIMER_CLOCK_TEST_SECONDS, TIMER_CLOCK_TEST_MAX_ERROR> TestAsync(SchedulerBase);

bool AllTestsOk = false;

void loop()
{
	SchedulerBase.execute();
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);

	Serial.println(F("LoLa Peripherals Tests starting."));

	AllTestsOk = true;

	TestEntropy();

	TestTimersBlocking();

	TestClocksAsync();
}

void TestEntropy()
{

	if (TestEntropySource(ArduinoEntropySource))
	{
		Serial.println(F("ArduinoEntropySource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("ArduinoEntropySource Fail."));
	}

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
	if (TestEntropySource(Stm32Entropy))
	{
		Serial.println(F("Stm32EntropySource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Stm32EntropySource Fail."));
	}
#endif

#if defined(ARDUINO_ESP8266)
	if (TestEntropySource(Esp8266Entropy))
	{
		Serial.println(F("Esp8266EntropySource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp8266EntropySource Fail."));
	}
#endif

#if defined(ARDUINO_ESP32)
	if (TestEntropySource(Esp32Entropy))
	{
		Serial.println(F("Esp32EntropySource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp32EntropySource Fail."));
	}
#endif
}

void OnTestAsyncComplete(const bool success)
{
	Serial.println();
	Serial.println();
	if (success && AllTestsOk)
	{
		Serial.println(F("LoLa Peripherals Tests completed with success."));
	}
	else
	{
		Serial.println(F("LoLa Peripherals Tests FAILED."));
	}
	Serial.println();
}


void TestTimersBlocking()
{
	if (TestTimerSource(ArduinoTimerClock))
	{
		Serial.println(F("ArduinoTimerSource Pass."));
	}
	else
	{
		Serial.println(F("ArduinoTimerSource Fail."));
		AllTestsOk = false;
	}

#if defined(ARDUINO_ARCH_AVR)
	if (TestTimerSource(AvrTimer))
	{
		Serial.println(F("AvrTimerSource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("AvrTimerSource Fail."));
	}
#endif

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
	if (TestTimerSource(Stm32Timer))
	{
		Serial.println(F("Stm32TimerSource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Stm32TimerSource Fail."));
	}
#endif

#if defined(ARDUINO_ESP8266)
	if (TestTimerSource(Esp8266Entropy))
	{
		Serial.println(F("Esp8266TimerSource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp8266TimerSource Fail."));
	}
#endif

#if defined(ARDUINO_ESP32)
	if (TestTimerSource(Esp32Entropy))
	{
		Serial.println(F("Esp32TimerSource Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp32TimerSource Fail."));
	}
#endif
}

void TestClocksAsync()
{
	TestAsync.StartClockTestTask(&ArduinoTimerClock, OnTestClocksCompleteArduino);
}

void OnTestClocksCompleteArduino(const bool success)
{
	if (success)
	{
		Serial.println(F("ArduinoClockSource Test Pass."));
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("ArduinoClockSource Test FAILED."));
	}

#if defined(ARDUINO_ARCH_AVR)
	if (!TestAsync.StartClockTestTask(&AvrClock, OnTestClocksCompleteAvr))
	{
		OnTestClocksCompleteAvr(false);
	}
#else
	OnTestClocksCompleteAvr(true);
#endif

}

void OnTestClocksCompleteAvr(const bool success)
{
	if (success)
	{
#if defined(ARDUINO_ARCH_AVR)
		Serial.println(F("AvrClockSource Test Pass."));
#endif
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("AvrClockSource Test FAILED."));
	}

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
	if (!TestAsync.StartClockTestTask(&Stm32Clock, OnTestClocksCompleteStm32))
	{
		OnTestClocksCompleteStm32(false);
	}
#else
	OnTestClocksCompleteStm32(true);
#endif
}

void OnTestClocksCompleteStm32(bool success)
{
	if (success)
	{
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
		Serial.println(F("Stm32ClockSource Test Pass."));
#endif
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Stm32ClockSource Test FAILED."));
	}

#if defined(ARDUINO_ESP8266)
	if (!TestAsync.StartClockTestTask(&Esp8266Clock, OnTestClocksCompleteEsp8266))
	{
		OnTestClocksCompleteEsp8266(false);
	}
#else
	OnTestClocksCompleteEsp8266(true);
#endif
}

void OnTestClocksCompleteEsp8266(bool success)
{
	if (success)
	{
#if defined(ARDUINO_ESP8266)
		Serial.println(F("Esp8266ClockSource Test Pass."));
#endif
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp8266ClockSource Tests FAILED."));
	}

#if defined(ARDUINO_ESP32)
	if (TestAsync.StartClockTestTask(&Esp8266Clock, OnTestClocksCompleteEsp32))
	{
		OnTestClocksCompleteEsp32(false);
	}
#else
	OnTestClocksCompleteEsp32(true);
#endif
}

void OnTestClocksCompleteEsp32(bool success)
{
	if (success)
	{
#if defined(ARDUINO_ESP32)
		Serial.println(F("Esp32ClockSource Test Pass."));
#endif
	}
	else
	{
		AllTestsOk = false;
		Serial.println(F("Esp32ClockSource Test FAILED."));

	}
	OnTestAsyncComplete(true);
}