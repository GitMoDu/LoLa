// RadioScanner.h

#ifndef _RADIOSCANNER_h
#define _RADIOSCANNER_h

//#include <Arduino.h>
#include <Stream.h>
#include <Si446x.h>

class RadioScanner
{
public:
	RadioScanner()
	{

	}

	void Scan(Stream* serialOutput = nullptr)
	{
		uint32_t Start, End;

		uint8_t peakChannel = 0;
		int16_t peakRssi = -999;
		int16_t peakChannelRssi;
		uint8_t peakChannelCandidate;

		uint8_t valleyChannel = 0;
		int16_t valleyRssi = 0;
		int16_t valleyChannelRssi;
		uint8_t valleyChannelCandidate;

		int16_t rssi;

		Start = micros();
		for (uint8_t i = 0; i < 255; i++)
		{
			valleyChannelRssi = 0;
			valleyChannelCandidate = 0;
			peakChannelRssi = -999;
			peakChannelCandidate = 0;
			Si446x_RX(i);
			for (uint8_t j = 0; j < 20; j++)
			{
				rssi = Si446x_getRSSI();
				if (rssi > peakChannelRssi)
				{
					peakChannelRssi = rssi;
					peakChannelCandidate = i;
				}
				if (rssi < valleyChannelRssi)
				{
					valleyChannelRssi = rssi;
					valleyChannelCandidate = i;
				}
			}

			if (peakChannelRssi > peakRssi)
			{
				peakChannel = peakChannelCandidate;
				peakRssi = peakChannelRssi;
			}

			if (valleyChannelRssi < valleyRssi)
			{
				valleyChannel = valleyChannelCandidate;
				valleyRssi = valleyChannelRssi;
			}
		}
		End = micros();

		/*if (serialOutput != nullptr)
		{
			serialOutput->print(F("Peak Rssi\t"));
			serialOutput->print(peakRssi);
			serialOutput->print(F("\tat\t"));
			serialOutput->print(peakChannel);
			serialOutput->print(F("\tValley Rssi\t"));
			serialOutput->print(valleyRssi);
			serialOutput->print(F("\tat\t"));
			serialOutput->print(valleyChannel);
			serialOutput->print(F("\tTook\t"));
			serialOutput->print((End - Start) / 1000.0f, 2);
			serialOutput->print(F(" ms"));

			serialOutput->println();
		}		*/
	}
private:

};


#endif

