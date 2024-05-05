// EspNowTransceiver.h
// For use with ESP8266 devices.
// Uses Wrapper library: WifiEspNow - https://github.com/yoursunny/WifiEspNow

#ifndef _ESP_8266_NOW_TRANSCEIVER_h
#define _ESP_8266_NOW_TRANSCEIVER_h

#if defined(ARDUINO_ARCH_ESP8266)
#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "../ILoLaTransceiver.h"
#include <WifiEspNow.h>

/// <summary>
/// Wrapped implementation for ESP8266 and ESP32.
/// Wrapper: WifiEspNow - https://github.com/yoursunny/WifiEspNow
/// TODO: Implement Rx and listen.
/// TODO: Make it actually work.
/// TODO: Read RSSI with callback https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino
/// TODO: Check how many channels in ESP-NOW mode https://github.com/espressif/esp-idf/issues/9592
/// </summary>
/// <typeparam name="DataRateCode"></typeparam>
//template<const wifi_phy_rate_t DataRateCode>
class EspNowTransceiver final
	: private Task, public virtual ILoLaTransceiver
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0x3403;

	static constexpr uint8_t CHANNEL = 0;
	static constexpr uint8_t ChannelCount = 13;

	ILoLaTransceiverListener* Listener = nullptr;

	//using RxCallback = void (*)(const uint8_t mac[WIFIESPNOW_ALEN], const uint8_t* buf, size_t count, void* arg);
	using RxCallback = void (*)(const uint8_t* mac, const uint8_t* buf, size_t count, void* arg);

private:
	RxCallback RxInterrupt = nullptr;

private:
	WifiEspNowPeerInfo Peers[1]{};
	uint8_t mac[WIFIESPNOW_ALEN]{};

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};
	const char ESP_NOW[6] = { 'E','S','P','N','O','W' };

	uint32_t TxTimestamp = 0;
	volatile uint32_t RxTimestamp = 0;
	volatile uint8_t RxSize = 0;

	bool TxPending = false;
	//volatile bool TxEvent = false;

public:
	EspNowTransceiver(Scheduler& scheduler)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}

public:
	virtual bool Callback() final
	{
		if (TxPending)
		{
			const WifiEspNowSendStatus sendStatus = WifiEspNow.getSendStatus();

			switch (sendStatus)
			{
			case WifiEspNowSendStatus::OK:
				Serial.println(F("WifiEspNow Tx Ok"));
				Listener->OnTx();
				TxPending = false;
				break;
			case WifiEspNowSendStatus::FAIL:
				Serial.println(F("WifiEspNow Tx Fail"));

				// Tx Fail.
				Listener->OnTx();
				TxPending = false;
				break;
			case WifiEspNowSendStatus::NONE:
			default:
				if ((millis() - TxTimestamp) > 30)
				{
					Serial.println(F("WifiEspNow Tx Timeout"));
					Listener->OnTx();
					TxPending = false;
				}
				break;
			}
		}

		if (RxSize > 0)
		{
			Listener->OnRx(InBuffer, RxTimestamp, RxSize, 0);
			RxSize = 0;
		}

		if (TxPending
			|| RxSize > 0)
		{
			Task::enable();

			return true;
		}
		else
		{
			Task::disable();
			return false;
		}
	}

	void SetupInterrupts(RxCallback onRxInterrupt)
	{
		RxInterrupt = onRxInterrupt;
	}

	void OnRxInterrupt(const uint8_t* mac, const uint8_t* buf, size_t count)
	{
		RxTimestamp = micros();

		if (count < LoLaPacketDefinition::MIN_PACKET_SIZE
			|| count > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			// Invalid size.
			Listener->OnRxLost(RxTimestamp);
		}

		for (uint_fast8_t i = 0; i < WIFIESPNOW_ALEN; i++)
		{
			if (mac[i] != UINT8_MAX) {
				// Mac mismatch.
				Listener->OnRxLost(RxTimestamp);
				return;
			}
		}

		// Copy to buffer.
		for (uint_fast8_t i = 0; i < count; i++)
		{
			InBuffer[i] = buf[i];
		}
		RxSize = count;
		Task::enable();
	}

public:
	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;//&& LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE <= ESP_NOW_MAX_DATA_LEN;
	}

	virtual const bool Start() final
	{
		if (Listener != nullptr
			//&& OnDataSent != nullptr
			)
		{
			WiFi.persistent(false);
			WiFi.mode(WIFI_AP);
			WiFi.disconnect();
			WiFi.softAP(ESP_NOW, nullptr, CHANNEL);
			//WiFi.softAP("ESPNOW", nullptr, CHANNEL);
			WiFi.softAPdisconnect(false);

			for (uint8_t i = 0; i < WIFIESPNOW_ALEN; i++)
			{
				mac[i] = UINT8_MAX;
			}
			WiFi.softAPmacAddress(mac);

			const bool ok = WifiEspNow.begin();
			if (!ok) {
				Serial.println(F("WifiEspNow.begin() failed"));
				ESP.restart();

				if (!WifiEspNow.begin())
				{
					return false;
				}
			}

			WifiEspNow.removePeer(mac);

			if (!WifiEspNow.addPeer(mac, CHANNEL, nullptr))
			{
				return false;
			}

			WifiEspNow.onReceive(RxInterrupt, nullptr);

			const uint8_t peerCount = WifiEspNow.listPeers(Peers, 1);

			if (peerCount != 1)
			{
				return false;
			}

			Peers[0].channel = CHANNEL;

			return true;
		}

		return false;
	}

	virtual const bool Stop() final
	{
		WifiEspNow.removePeer(mac);
		WifiEspNow.end();
		WiFi.disconnect();
		Task::disable();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return !TxPending;
	}

	virtual const uint32_t GetTransceiverCode()
	{
		return (uint32_t)TRANSCEIVER_ID //+ (((uint32_t)AddressPipe + 1) * AddressSize)
			//| (uint32_t)DataRateCode << 16
			| (uint32_t)ChannelCount << 24
			| (uint32_t)CHANNEL << 24;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// The first byte is the packet size, excluding the size byte.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns>True if transmission was successful.</returns> 
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		if (TxAvailable())
		{
			// Can we change this without stopping AP?
			//WifiEspNow(channel);
			if (WifiEspNow.send(mac, data, packetSize))
			{
				TxTimestamp = millis();
				TxPending = true;
				Task::enable();
			}
			else
			{
				Serial.println(F("ESP-Now Transmit Failed."));
			}
		}

		return false;
	}


	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		//esp_wifi_set_channel(0, WIFI_SECOND_CHAN_NONE);

		Task::enable();
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return 0;
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return 2000;
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return GetRealChannel<ChannelCount>(abstractChannel);
	}
};
#endif
#endif