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


//#if defined(DEBUG_LOLA)
//Serial.println(F("Random Seeding complete."));
//if (idSize > 0)
//{
//	Serial.print(F("\tUnique Id ("));
//	Serial.print(idSize);
//	Serial.println(F("bytes):"));

//	Serial.println('\t');
//	Serial.print('|');
//	Serial.print('|');
//	for (size_t i = 0; i < idSize; i++)
//	{
//		Serial.print(F("0x"));
//		if (uniqueId[i] < 0x10)
//		{
//			Serial.print(0);
//		}
//		Serial.print(uniqueId[i], HEX);
//		Serial.print('|');
//	}
//	Serial.println('|');
//}

//Serial.print(F("\tSeed noise: "));
//Serial.println(EntropySource->GetNoise());
//Serial.print(F("\tFirst Random: "));
//Serial.println(GetNextRandom());

//uint32_t sum = 0;
//const uint32_t start = micros();
//static const uint32_t samples = 1000;
//for (uint32_t i = 0; i < samples; i++)
//{
//	sum += GetNextRandom();
//}
//const uint32_t duration = micros() - start;

//Serial.println(F("\tBenchmark "));
//Serial.print(samples);
//Serial.println(F(" samples took  "));
//Serial.print(duration);
//Serial.print(F(" us ("));
//Serial.print(((uint64_t)duration * 1000) / (samples * sizeof(uint32_t)));
//Serial.println(F(" ns/byte )"));
//#endif
#endif