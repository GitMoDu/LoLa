// Tests.h

#ifndef _TESTS_h
#define _TESTS_h

#define LOLA_UNIT_TESTING

#include <ILoLaInclude.h>

const bool TestEntropySource(IEntropy& entropySource)
{
	Serial.print(F("\tFixed value:"));
	uint8_t idSize = 0;
	const uint8_t* uniqueId = entropySource.GetUniqueId(idSize);

	for (size_t i = 0; i < idSize; i++)
	{
		Serial.print(uniqueId[i], HEX);
		Serial.print(F(","));
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

const bool TestTimerSource(ITimerSource& testSource)
{
	static const uint32_t TIME_TEST_DELAY_MICROS = 5000000;

	testSource.StartTimer();

	uint32_t time1;
	uint32_t time2;
	int64_t timeDelta;
	int64_t timeError;

	delay(1);

	// Get a timestamp, block wait with arduino millis and get another timestamp.
	time1 = testSource.GetCounter();
	delayMicroseconds(TIME_TEST_DELAY_MICROS);
	time2 = testSource.GetCounter();

	testSource.StopTimer();

	timeDelta = ((uint64_t)((uint32_t)(time2 - time1)) * ONE_SECOND_MICROS) / testSource.GetDefaultRollover();
	timeError = (int64_t)TIME_TEST_DELAY_MICROS - timeDelta;


	if (timeError < 0) {
		timeError = -timeError;
	}

	const uint64_t ppm = (timeError * 1000000) / TIME_TEST_DELAY_MICROS;
	Serial.print(F("\tTimer error:"));
	Serial.print(timeError);
	Serial.print(F("us\tPPM: "));
	Serial.println(ppm);
	if (ppm > 50) {
		return false;
	}

	return true;
}
#endif