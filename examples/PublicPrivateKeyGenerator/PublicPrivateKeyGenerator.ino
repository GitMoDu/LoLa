/* LoLa Link Key generator.
* Generate a one time pair of private/public keys for ECC Public-Key Exchange.
* This means the secret key between 2 devices will be constant,
* so there is no need to calculate it every session (> 10 ms operation).
*
* The sketch also benchmarks commons operations.
*/


#define WAIT_FOR_LOGGER
#define SERIAL_BAUD_RATE 115200


#include <uECC.h>
#include <ILoLaInclude.h>

#define LINK_DUPLEX_SLOT false
#include "Testing\ExampleTransceiverDefinitions.h"

uint8_t Private1[21];
uint8_t Private2[21];

uint8_t Public1[40];
uint8_t Public2[40];

uint8_t Secret1[20];
uint8_t Secret2[20];

uint8_t Public1Compressed[21];
uint8_t Public2Compressed[21];

uint8_t Public1Decompressed[40];
uint8_t Public2Decompressed[40];


const struct uECC_Curve_t* ECC_CURVE = uECC_secp160r1();

// Use best available sources.
static const uint8_t EntropyPin = PA1;

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
#ifdef WAIT_FOR_LOGGER
	while (!Serial)
		;
	delay(1000);
#endif

	uECC_set_rng(&RNG);

	Serial.println(F("LoLa Link Key Generator Start!"));
}

void PrintKey(uint8_t* key, const size_t size)
{
	Serial.print(' ');
	Serial.print('{');
	for (size_t i = 0; i < size; i++)
	{
		Serial.print(0);
		Serial.print('x');
		if (key[i] < 0x10) {
			Serial.print(0);
		}
		Serial.print(key[i], HEX);
		if (i < size - 1)
		{
			Serial.print(',');
		}
	}
	Serial.print('}');
	Serial.println(';');
}
void loop()
{
	uint32_t start = micros();
	uint32_t end = 0;
	uECC_make_key(Public1, Private1, ECC_CURVE);
	uECC_make_key(Public2, Private2, ECC_CURVE);
	end = micros();

	Serial.print(F("Key generation took "));
	Serial.print((end - start) / 2);
	Serial.println(F((" us.")));

	Serial.print(F(("Public Key ")));
	PrintKey(Public1, 40);

	Serial.print(F(("Private Key ")));
	PrintKey(Private1, 21);

	bool success = false;
	start = micros();
	success = uECC_shared_secret(Public2, Private1, Secret1, ECC_CURVE);
	success &= uECC_shared_secret(Public1, Private2, Secret2, ECC_CURVE);
	end = micros();

	if (success && ArrayMatches(Secret1, Secret2, 20))
	{
		Serial.print(F("Secret Key calculation took "));
		Serial.print((end - start) / 2);
		Serial.println(F((" us.")));
	}
	else
	{
		Serial.println(F("Secret Key calculation failed."));
	}

	start = micros();
	uECC_compress(Public1, Public1Compressed, ECC_CURVE);
	uECC_compress(Public2, Public2Compressed, ECC_CURVE);
	end = micros();

	Serial.print(F("Public Key compression took "));
	Serial.print((end - start) / 2);
	Serial.println(F((" us.")));

	start = micros();
	uECC_decompress(Public1Compressed, Public1Decompressed, ECC_CURVE);
	uECC_decompress(Public2Compressed, Public2Decompressed, ECC_CURVE);
	end = micros();

	if (success
		&& ArrayMatches(Public1, Public1Decompressed, 40)
		&& ArrayMatches(Public2, Public2Decompressed, 40))
	{
		Serial.print(F("Public Key decompression took "));
		Serial.print((end - start) / 2);
		Serial.println(F((" us.")));
	}
	else
	{
		Serial.println(F("Public Key compression/decompression failed."));
	}

	delay(5000);
}

static int RNG(uint8_t* dest, unsigned size)
{
	for (size_t i = 0; i < size; i++)
	{
		dest[i] = EntropySource.GetNoise() % UINT8_MAX;
	}

	return 1;
}

const bool ArrayMatches(uint8_t* a, uint8_t* b, const size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		if (a[i] != b[i]) {
			return false;
		}
	}
	return true;
}