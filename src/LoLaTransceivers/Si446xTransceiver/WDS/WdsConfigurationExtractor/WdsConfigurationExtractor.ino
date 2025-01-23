/* WDS Configuration extractor.
* Prints the unfolded configuration data-array to serial.
* Wireless Development Suite should be available at https://www.silabs.com/support/resources.ct-software.p-wireless_proprietary_ezradiopro-sub-ghz-wireless-ics_si4463
* Instructions
*	WDS
*	- Export custom project for third party IDE
*	Copy to local project
*		- Si446x_EmptyFramework\src\application\radio_config.h
*		- Si446x_EmptyFramework\src\drivers\radio\Si446x\si446x_patch.h
*	Edit files, IF radio_config.h uses si446x_patch.h (Rev C2)
*	- radio_config_local.h
*		Replace #include "..\drivers\radio\Si446x\si446x_patch.h" with "si446x_patch.h.
*	Run sketch and copy the result from serial output.
*	Resulting array includes Patch and Config.
*/


#define SERIAL_BAUD_RATE 115200

#include <Arduino.h>

#include "radio_config.h"
#include "si446x_patch.h"


static constexpr uint8_t Config[] RADIO_CONFIGURATION_DATA_ARRAY;
static constexpr size_t ConfigSize = sizeof(Config);

#if defined(SI446X_PATCH_CMDS)
static constexpr uint8_t Patch[][8]{ SI446X_PATCH_CMDS };
static constexpr size_t PatchSize = sizeof(Patch);
#endif

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);

	Serial.println(F("Si446x WDS configuration extractor."));

	Serial.println();
	PrintConfig();
	Serial.println();

#if defined(SI446X_PATCH_CMDS)
	Serial.println();
	PrintPatch();
	Serial.println();
#endif
}

void loop()
{
}

void PrintHex(const uint8_t value)
{
	Serial.print(F("0x"));
	if (value < 0x10)
	{
		Serial.print(0, HEX);
	}
	Serial.print(value, HEX);
}

const size_t PrintArrayItem(const uint8_t* itemArray, const bool isLastItem = false)
{
	const size_t size = itemArray[0];

	Serial.print('\t');
	Serial.print('\t');
	PrintHex(size);
	if (size > 0)
	{
		Serial.print(',');
		Serial.print(' ');
	}

	for (size_t i = 0; i < size; i++)
	{
		PrintHex(itemArray[i + 1]);
		if (i < size - 1)
		{
			Serial.print(',');
			Serial.print(' ');
		}
	}
	if (!isLastItem)
	{
		Serial.println(',');
	}

	return size;
}

void PrintConfig()
{
	Serial.println(F("static constexpr uint8_t SetupConfig[]\n{"));

	size_t i = 0;
	while (i < ConfigSize)
	{
		const size_t length = Config[i];

		PrintArrayItem(&Config[i], i + length >= ConfigSize - 1);
		i += 1 + length;
	}

	Serial.println();
	Serial.println(F("};"));
}

#if defined(SI446X_PATCH_CMDS)
void PrintPatch()
{
	Serial.println(F("static constexpr uint8_t RomPatch[]\n{"));

	for (size_t i = 0; i < PatchSize / 8; i++)
	{
		Serial.print('\t');
		Serial.print('\t');

		for (size_t j = 0; j < 8; j++)
		{
			PrintHex(Patch[i][j]);
			if (j < 8 - 1)
			{
				Serial.print(',');
			}
		}
		if (i < (PatchSize / 8) - 1)
		{
			Serial.println(',');
		}
	}

	Serial.println();
	Serial.println(F("};"));
}
#endif
