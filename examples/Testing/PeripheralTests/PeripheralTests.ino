/* Test Arduino LoLa Peripheral Drivers
*
*/


#define WAIT_FOR_LOGGER
#define SERIAL_BAUD_RATE 115200

#include <ILoLaInclude.h>

ArduinoEntropySource EntropySource(123);


ArduinoMicrosClock<> TimerArduino;
ArduinoMicrosClock<987654> TimerArduino2;


#if defined(ARDUINO_ARCH_AVR)
AvrTimer1Clock TimerAvr;
#endif

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
static const uint8_t TimerStm32Index = 1;
static const uint8_t TimerStm32Channel = 'A';
Stm32TimerClock TimerStm32(TimerStm32Index, TimerStm32Channel);
#endif

#if defined(ARDUINO_ESP8266)
static const uint8_t TimerEsp8266Index = 1;
Esp8266TimerClock TimerEsp8266(TimerEsp8266Index);
#endif

#if defined(ARDUINO_ESP32)
static const uint8_t TimerEsp32Index = 1;
Esp32TimerClock TimerEsp32(TimerEsp32Index);
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

	if (!TestEntropySource(EntropySource))
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

	ArrayToUint32 atui;

	for (size_t i = 0; i < idSize; i = i + 4)
	{
		atui.Array[0] = uniqueId[i];
		atui.Array[1] = uniqueId[i + 1];
		atui.Array[2] = uniqueId[i + 2];
		atui.Array[3] = uniqueId[i + 3];
		Serial.print(atui.UInt);
		if ((i * 4) < idSize)
		{
			Serial.println(F(",\t"));
		}
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
	if (!TestTimerSource(TimerArduino2, "Arduino2"))
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
const bool TestTimerSource(ITimerSource& testSource, char* name)
{
	static ITimerSource::RollingTimeStruct time1;
	static ITimerSource::RollingTimeStruct time2;
	static int64_t timeDelta;
	static int64_t timeError;

	delay(1);

	// Get a timestamp, block wait with arduino millis and get another timestamp.
	testSource.GetTime(time1);
	delay(TIME_TEST_DELAY_MILLIS);
	testSource.GetTime(time2);

	timeDelta = time1.GetDeltaTo(time2);
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
