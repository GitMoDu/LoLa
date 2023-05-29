/* Test LoLa Peripheral Drivers.
*
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#endif

#define SERIAL_BAUD_RATE 115200

#define _TASK_SCHEDULE_NC // Disable task catch-up.
#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>

#include <ILoLaInclude.h>

// Process scheduler.
Scheduler SchedulerBase;
//

ArduinoEntropySource ArduinoEntropy(123);
ArduinoTaskTimerClockSource<> TimerArduino(SchedulerBase);


#if defined(ARDUINO_ARCH_AVR)
AvrTimer1Clock TimerAvr;
#endif

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource Stm32Entropy;

IClockSource* ServerClock = &TimerArduino;
ITimerSource* ServerTimer = &TimerArduino;
//static const uint8_t TimerStm32Index = 1;
//static const uint8_t TimerStm32Channel = 'A';
#endif

#if defined(ARDUINO_ESP8266)
IClockSource* ServerClock = &TimerArduino;
ITimerSource* ServerTimer = &TimerArduino;
//static const uint8_t TimerEsp8266Index = 1;
//Esp8266TimerClock TimerEsp8266(TimerEsp8266Index);
#endif

#if defined(ARDUINO_ESP32)
IClockSource* ServerClock = &TimerArduino;
ITimerSource* ServerTimer = &TimerArduino;
//static const uint8_t TimerEsp32Index = 1;
//Esp32TimerClock TimerEsp32(TimerEsp32Index);
#endif



static const uint32_t TIME_TEST_DELAY_MILLIS = 3124;


void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
#ifdef WAIT_FOR_LOGGER
	while (!Serial)
		;
	delay(1000);
#endif


	Serial.println(F("Test Arduino LoLa Peripheral Drivers Start!"));


	Serial.println();
	Serial.println();
	Serial.println(F("Testing Entropy."));

	if (!TestEntropySource(ArduinoEntropy))
	{
		return;
	}	
	
	if (!TestEntropySource(Stm32Entropy))
	{
		return;
	}

	if (!TestTimerSources())
	{
		return;
	}


	Serial.println(F("Test Arduino LoLa Peripheral Drivers Finished!"));
}


const bool TestEntropySource(IEntropySource& entropySource)
{
	Serial.println(F("\tFixed value:"));
	uint8_t idSize = 0;
	const uint8_t* uniqueId = entropySource.GetUniqueId(idSize);

	for (size_t i = 0; i < idSize; i = i + 4)
	{
		Serial.print(uniqueId[i]);
		Serial.println(F(",\t"));
	}

	uint32_t samplePrevious = 0;
	uint32_t sample = entropySource.GetNoise();

	Serial.println();
	Serial.print(F("\tRandom sample 1: "));
	Serial.println(sample);


	sample = entropySource.GetNoise();
	if (sample == samplePrevious) {
		Serial.print(F("\tRandom sample 2 failed."));
		return false;
	}
	samplePrevious = sample;
	Serial.print(F("\tRandom sample 2: "));
	Serial.println(sample);

	sample = entropySource.GetNoise();
	if (sample == samplePrevious) {
		Serial.print(F("\tRandom sample 3 failed."));
		return false;
	}
	samplePrevious = sample;
	Serial.print(F("\tRandom sample 3: "));
	Serial.println(sample);

	return true;
}


const bool TestTimerSources()
{
	Serial.println();
	Serial.println();
	Serial.print(F("Testing Timer Sources ("));
	Serial.print(TIME_TEST_DELAY_MILLIS / 1000);
	Serial.println(F(" s each)."));

	if (!TestTimerSource(TimerArduino, "Arduino"))
	{
		return false;
	}

#if defined(ARDUINO_ARCH_AVR)
	if (!TestTimerSource(TimerArduino, "AVR"))
	{
		return false;
	}
#endif

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F1)
	if (!TestTimerSource(TimerArduino, "Arduino"))
		//if (!TestTimerSource(TimerStm32, "Stm32"))
	{
		return false;
	}
#endif

#if defined(ARDUINO_ESP8266)
	if (!TestTimerSource(TimerEsp8266, "Esp8266"))
	{
		return false;
	}
#endif
#if defined(ARDUINO_ESP32)
	if (!TestTimerSource(TimerEsp32, "Esp32"))
	{
		return false;
	}
#endif

	return true;
	}
const bool TestTimerSource(ITimerSource& testSource, String name)
{
	static uint32_t time1;
	static uint32_t time2;
	static int64_t timeDelta;
	static int64_t timeError;

	delay(1);

	// Get a timestamp, block wait with arduino millis and get another timestamp.
	time1 = testSource.GetCounter();
	delay(TIME_TEST_DELAY_MILLIS);
	time2 = testSource.GetCounter();

	timeDelta = ((uint64_t)(time2 - time1) * ONE_SECOND_MICROS) / testSource.GetDefaultRollover();
	timeError = ((int64_t)TIME_TEST_DELAY_MILLIS * 1000) - timeDelta;


	if (timeError < 0) {
		timeError = -timeError;
	}

	const uint64_t ppb = timeError * 1000000 / TIME_TEST_DELAY_MILLIS;
	Serial.print(F("\tTimer "));
	Serial.print(name);
	Serial.print(F(" error: "));
	Serial.print(timeError);
	Serial.print(F("us\tPPB: "));
	Serial.println(ppb);
	if (ppb > 5000) {
		Serial.print(F("\tTimer "));
		Serial.print(name);
		Serial.print(F("failed test."));
		return false;
	}

	return true;
}

void loop()
{
}
