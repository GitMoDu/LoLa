// EspNowTransceiver.h
// For use with ESP-NOW (tm) device ESP32.

#ifndef _ESP_32_NOW_TRANSCEIVER_h
#define _ESP_32_NOW_TRANSCEIVER_h

#if defined(ARDUINO_ARCH_ESP32)
#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "../ILoLaTransceiver.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // only for esp_wifi_set_channel()

/// <summary>
/// ESP32 implementation for Expressif Arduino core.
/// TODO: Read RSSI with callback https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino
/// TODO: Check how many channels in ESP-NOW mode https://github.com/espressif/esp-idf/issues/9592
/// TODO: Check if power level is working and abstract power levels.
/// TODO: Use broadcast mode.
/// TODO: Remove Ack (retry count = 0).
/// </summary>
class EspNowTransceiver final
	: public virtual ILoLaTransceiver
	, private TS::Task
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0x3403;

	static constexpr uint8_t ChannelCount = 15;

	static constexpr uint8_t CHANNEL = ChannelCount / 2;

	static constexpr wifi_phy_rate_t PhyRate = WIFI_PHY_RATE_2M_S;

	ILoLaTransceiverListener* Listener = nullptr;



private:
	void (*TxInterrupt)(const uint8_t* mac_addr, esp_now_send_status_t status) = nullptr;
	void (*RxInterrupt)(const uint8_t* mac_addr, const uint8_t* data, int data_len) = nullptr;

private:
	esp_now_peer_info_t slave;


private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	volatile uint32_t RxTimestamp = 0;
	volatile uint8_t RxSize = 0;

	bool TxPending = false;
	volatile bool TxEvent = false;

public:
	EspNowTransceiver(TS::Scheduler& scheduler)
		: ILoLaTransceiver()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}

public:
	virtual bool Callback() final
	{
		if (TxEvent)
		{
			TxEvent = false;
			if (TxPending)
			{
				Listener->OnTx();
				TxPending = false;
			}
		}

		if (RxSize > 0)
		{
			Listener->OnRx(InBuffer, RxTimestamp, RxSize, 0);
			RxSize = 0;
		}

		if (TxEvent || RxSize > 0)
		{
			TS::Task::enable();

			return true;
		}
		else
		{
			TS::Task::disable();
			return false;
		}

	}

	void SetupInterrupts(void (*onRxInterrupt)(const uint8_t* mac_addr, const uint8_t* data, int data_len),
		void (*onTxInterrupt)(const uint8_t* mac_addr, esp_now_send_status_t status))
	{
		RxInterrupt = onRxInterrupt;
		TxInterrupt = onTxInterrupt;
	}

	void OnTxInterrupt(esp_now_send_status_t status)
	{
		TxEvent = true;
		TS::Task::enable();
	}

	void OnRxInterrupt(const uint8_t* data, int data_len)
	{
		RxTimestamp = micros();

		if (data_len < LoLaPacketDefinition::MIN_PACKET_SIZE
			|| data_len > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			// Invalid size.
		}

		// Copy to buffer.
		for (uint_fast8_t i = 0; i < data_len; i++)
		{
			InBuffer[i] = data[i];
		}
		RxSize = data_len;
		TS::Task::enable();
	}

public:
	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr && LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE <= ESP_NOW_MAX_DATA_LEN;
	}

	virtual const bool Start() final
	{
		if (Listener != nullptr
			&& RxInterrupt != nullptr
			&& TxInterrupt != nullptr
			)
		{
			WiFi.mode(WIFI_STA);
			WiFi.disconnect(false, true);


			if (esp_now_init() != ESP_OK)
			{
				Serial.println(F("Setup error, going once..."));
				ESP.restart();
				if (esp_now_init() != ESP_OK)
				{
					return false;
				}
			}

			if (esp_now_register_send_cb(TxInterrupt) != ESP_OK)
			{
				return false;
			}

			if (esp_now_register_recv_cb(RxInterrupt) != ESP_OK)
			{
				return false;
			}

			initBroadcastSlave();

			if (!manageSlave())
			{
				return false;
			}

			if (esp_wifi_start() != ESP_OK)
			{
				return false;
			}

			esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

			//esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
			esp_wifi_set_max_tx_power(WIFI_POWER_MINUS_1dBm);

			if (esp_wifi_config_espnow_rate(slave.ifidx, PhyRate) != ESP_OK)
			{
				return false;
			}

			return true;
		}

		return false;
	}

	virtual const bool Stop() final
	{
		deletePeer();
		WiFi.disconnect();
		esp_wifi_stop();
		TS::Task::disable();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return !TxPending;
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return (uint32_t)TRANSCEIVER_ID
			| (((uint32_t)(PhyRate)
				^ (uint32_t)(ChannelCount)
				^ (uint32_t)(CHANNEL)) << 16);
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
			//esp_wifi_set_channel(GetRawChannel(channel), WIFI_SECOND_CHAN_NONE);
			esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

			const uint8_t* peer_addr = slave.peer_addr;
			const esp_err_t result = esp_now_send(peer_addr, data, packetSize);

			switch (result)
			{
			case ESP_OK:
				TxPending = true;
				TS::Task::enable();
				return true;
				break;
			case ESP_ERR_ESPNOW_ARG:
				Serial.println(F("Invalid Argument"));
				break;
			case ESP_ERR_ESPNOW_INTERNAL:
				Serial.println(F("Internal Error"));
				break;
			case ESP_ERR_ESPNOW_NO_MEM:
				Serial.println(F("ESP_ERR_ESPNOW_NO_MEM"));
				break;
			case ESP_ERR_ESPNOW_NOT_FOUND:
				Serial.println(F("Peer not found."));
				break;
			case ESP_ERR_ESPNOW_NOT_INIT:
				Serial.println(F("ESPNOW not Init."));
				break;
			default:
				Serial.println(F("Not sure what happened"));
				break;
			}
		}

		return false;
	}


	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

		TS::Task::enable();
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return 1;
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

private:
	void initBroadcastSlave()
	{
		for (size_t i = 0; i < ESP_NOW_KEY_LEN; i++)
		{
			slave.lmk[i] = 0;
		}
		for (size_t i = 0; i < ESP_NOW_ETH_ALEN; i++)
		{
			slave.peer_addr[i] = (const uint8_t)UINT_MAX;
		}
		slave.channel = CHANNEL;
		slave.encrypt = false;
	}

	// Check if the slave is already paired with the master.
	// If not, pair the slave with master
	const bool manageSlave() {
		deletePeer();

		Serial.print("Slave Status: ");
		const esp_now_peer_info_t* peer = &slave;
		const uint8_t* peer_addr = slave.peer_addr;
		// check if the peer exists
		bool exists = esp_now_is_peer_exist(peer_addr);
		if (exists) {
			// Slave already paired.
			Serial.println("Already Paired");
			return true;
		}
		else {
			// Slave not paired, attempt pair
			esp_err_t addStatus = esp_now_add_peer(peer);
			if (addStatus == ESP_OK) {
				// Pair success
				Serial.println(F("Pair success"));
				return true;
			}
			else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
				// How did we get so far!!
				Serial.println(F("ESPNOW Not Init"));
				return false;
			}
			else if (addStatus == ESP_ERR_ESPNOW_ARG) {
				Serial.println(F("Invalid Argument"));
				return false;
			}
			else if (addStatus == ESP_ERR_ESPNOW_FULL) {
				Serial.println(F("Peer list full"));
				return false;
			}
			else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
				Serial.println(F("Out of memory"));
				return false;
			}
			else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
				Serial.println(F("Peer Exists"));
				return true;
			}
			else {
				Serial.println(F("Not sure what happened"));
				return false;
			}
		}
	}

	void deletePeer() {
		//const esp_now_peer_info_t* peer = &slave;
		const uint8_t* peer_addr = slave.peer_addr;
		esp_err_t delStatus = esp_now_del_peer(peer_addr);

		if (delStatus == ESP_OK) {
			// Delete success
			//Serial.println(F("Success"));
		}
		else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
			// How did we get so far!!
			Serial.println(F("Slave Delete Status: ESPNOW Not Init"));
		}
		else if (delStatus == ESP_ERR_ESPNOW_ARG) {
			Serial.println(F("Slave Delete Status: Invalid Argument"));
		}
		else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
			//Serial.println(F("Slave Delete Status: Peer not found."));
		}
		else {
			Serial.print("Slave Delete Status: ");
			Serial.println(F("Slave Delete Status: Not sure what happened"));
		}
	}
};
#endif
#endif