
#include "WdsExtractedConfiguration.h"
#define SERIAL_BAUD_RATE 115200

#define DEBUG_LOLA
#define LOLA_UNIT_TESTING


//#define LOLA_USE_POLY1305

#include <ILoLaInclude.h>
#include <Arduino.h>


// Replace file or contents with exported configuration from WDS editor.
// Removed/comment the generated #include "..\drivers\radio\Si446x\"
#include "WdsExtractedConfiguration.h"



using namespace WdsExtractedConfiguration;


static constexpr size_t ConfigurationSize = WdsExtractedConfiguration::GetFullSize();

static constexpr uint8_t FullArray[] = RADIO_CONFIGURATION_DATA_ARRAY;
static constexpr size_t FullArraySize = sizeof(FullArray);


void PrintHex(const uint8_t value)
{
	Serial.print(F("0x"));
	if (value < 0x10)
	{
		Serial.print(0, HEX);
	}
	Serial.print(value, HEX);
}


const size_t PrintConfigItem(const uint8_t* itemArray, const size_t size, const bool isLastItem = false)
{
	Serial.print('\t');
	Serial.print('\t');
	PrintHex(size);
	Serial.print(',');
	Serial.print(' ');

	for (size_t i = 0; i < size; i++)
	{
		PrintHex(itemArray[i + 1]);
		if (i < size - 1)
		{
			Serial.print(',');
		}
		Serial.print(' ');
	}
	if (!isLastItem)
	{
		Serial.println(',');
	}

	return size;
}


void PrintSetupConfig()
{
	Serial.println(F("static constexpr uint8_t SetupConfig[] = {"));

	size_t size = 0;

	size += PrintConfigItem(PowerUp, sizeof(PowerUp));
	size += PrintConfigItem(GpioPinCfg, sizeof(GpioPinCfg));
	size += PrintConfigItem(CrystalTune, sizeof(CrystalTune));
	size += PrintConfigItem(GlobalConfig1, sizeof(GlobalConfig1));
	size += PrintConfigItem(PreambleConfig, sizeof(PreambleConfig));
	size += PrintConfigItem(ModemType1, sizeof(ModemType1));
	size += PrintConfigItem(ModemFreqDeviation, sizeof(ModemFreqDeviation));
	size += PrintConfigItem(ModemTxRampDelay1, sizeof(ModemTxRampDelay1));
	size += PrintConfigItem(ModemBcrNcoOffset1, sizeof(ModemBcrNcoOffset1));
	size += PrintConfigItem(ModemAfcLimiter1, sizeof(ModemAfcLimiter1));
	size += PrintConfigItem(ModemAgcControl1, sizeof(ModemAgcControl1));
	size += PrintConfigItem(ModemAdcWindow1, sizeof(ModemAdcWindow1));
	size += PrintConfigItem(ModemRawControl1, sizeof(ModemRawControl1));
	size += PrintConfigItem(ModemRssiThreshold, sizeof(ModemRssiThreshold));
	size += PrintConfigItem(ModemRawSearch1, sizeof(ModemRawSearch1));
	size += PrintConfigItem(ModemSpikeDetect1, sizeof(ModemSpikeDetect1));
	size += PrintConfigItem(ModemRssiMute1, sizeof(ModemRssiMute1));
	size += PrintConfigItem(ModemDsaControl1, sizeof(ModemDsaControl1));
	size += PrintConfigItem(ModemChannelFilter1, sizeof(ModemChannelFilter1));
	size += PrintConfigItem(ModemChannelFilter2, sizeof(ModemChannelFilter2));
	size += PrintConfigItem(ModemChannelFilter3, sizeof(ModemChannelFilter3));
	size += PrintConfigItem(PaTuneControl, sizeof(PaTuneControl));
	size += PrintConfigItem(SynthPowerFactor1, sizeof(SynthPowerFactor1));
	size += PrintConfigItem(FrequencyControl1, sizeof(FrequencyControl1));
	size += PrintConfigItem(StartRx, sizeof(StartRx));
	size += PrintConfigItem(IrCalibration1, sizeof(IrCalibration1));
	size += PrintConfigItem(IrCalibration2, sizeof(IrCalibration2));
	size += PrintConfigItem(GlobalClockConfig, sizeof(GlobalClockConfig));
	size += PrintConfigItem(GlobalConfig2, sizeof(GlobalConfig2));
	size += PrintConfigItem(InterruptControlEnable, sizeof(InterruptControlEnable));
	size += PrintConfigItem(FrrControl, sizeof(FrrControl));
	size += PrintConfigItem(PreambleLength, sizeof(PreambleLength));
	size += PrintConfigItem(SyncConfig, sizeof(SyncConfig));
	size += PrintConfigItem(PacketCrcConfig, sizeof(PacketCrcConfig));
	size += PrintConfigItem(PacketRxThreshold, sizeof(PacketRxThreshold));
	size += PrintConfigItem(PacketFieldConfig1, sizeof(PacketFieldConfig1));
	size += PrintConfigItem(PacketFieldConfig2, sizeof(PacketFieldConfig2));
	size += PrintConfigItem(PacketFieldConfig3, sizeof(PacketFieldConfig3));
	size += PrintConfigItem(PacketCrcSeed, sizeof(PacketCrcSeed));
	size += PrintConfigItem(ModemModulationType, sizeof(ModemModulationType));
	size += PrintConfigItem(ModemFrequencyDeviation, sizeof(ModemFrequencyDeviation));
	size += PrintConfigItem(ModemTxRampDelay2, sizeof(ModemTxRampDelay2));
	size += PrintConfigItem(ModemBcrNcoOffset2, sizeof(ModemBcrNcoOffset2));
	size += PrintConfigItem(ModemAfcLimiter2, sizeof(ModemAfcLimiter2));
	size += PrintConfigItem(ModemAgcControl2, sizeof(ModemAgcControl2));
	size += PrintConfigItem(ModemAdcWindow2, sizeof(ModemAdcWindow2));
	size += PrintConfigItem(ModemRawControl2, sizeof(ModemRawControl2));
	size += PrintConfigItem(ModemRawSearch2, sizeof(ModemRawSearch2));
	size += PrintConfigItem(ModemSpikeDetect2, sizeof(ModemSpikeDetect2));
	size += PrintConfigItem(ModemRssiMute2, sizeof(ModemRssiMute2));
	size += PrintConfigItem(ModemDsaControl2, sizeof(ModemDsaControl2));
	size += PrintConfigItem(ModemChannelFilter4, sizeof(ModemChannelFilter4));
	size += PrintConfigItem(ModemChannelFilter5, sizeof(ModemChannelFilter5));
	size += PrintConfigItem(ModemChannelFilter6, sizeof(ModemChannelFilter6));
	size += PrintConfigItem(PaMode, sizeof(PaMode));
	size += PrintConfigItem(SynthPowerFactor2, sizeof(SynthPowerFactor2));
	size += PrintConfigItem(MatchValue, sizeof(MatchValue));
	size += PrintConfigItem(FrequencyControl2, sizeof(FrequencyControl2), true);
	Serial.println(F(" }"));


	/*bool success = false;
	while (i < ConfigurationSize)
	{
		if (GetResponse())
		{
			const size_t length = configuration[i];

			CsOn();
			SpiInstance.transfer((void*)&configuration[i + 1], length);
			CsOff();

			i += 1 + length;
			if (!success)
			{
				success = i == configurationSize;
				if (!success
					&& i > configurationSize)
				{
					return false;
				}
			}
		}
	}

	return success;*/


}


void Halt()
{
	Serial.println("[ERROR] Critical Error");
	delay(1000);

	while (1);;
}



void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);


	Serial.println(F("Si446x WDS configuration extractor starting:"));

	Serial.println();

	PrintSetupConfig();

	Serial.println();

	//const bool hasConfiguration = sizeof( ) > 0;


	/*
		Serial.println();
		Serial.println();
		if (allTestsOk)
		{
			Serial.println(F("LoLa Crypto Unit Tests completed with success."));
		}
		else
		{
			Serial.println(F("LoLa Crypto Unit Tests FAILED."));
		}*/
	Serial.println();
}

void loop()
{
}




