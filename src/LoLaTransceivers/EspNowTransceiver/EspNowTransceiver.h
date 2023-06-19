// EspNowTransceiverr.h
// For use with ESP-NOW (tm) compatible devices such as ESP8266 and ESP32.

#ifndef _ESP_NOW_TRANSCEIVER_h
#define _ESP_NOW_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

/// <summary>
/// TODO: Implement.
/// TODO: Read RSSI with callback https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino
/// TODO: Check how many channels in ESP-NOW mode https://github.com/espressif/esp-idf/issues/9592
/// </summary>
class EspNowTransceiverr : private Task, public virtual ILoLaTransceiver
{
private:
	static const uint8_t ChannelCount = 13;
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

public:
	EspNowTransceiverr(Scheduler& scheduler)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}

public:
	virtual bool Callback() final
	{
		Task::disable();
		return false;
	}

public:
	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		if (Listener != nullptr)
		{
			return false;
		}
		else
		{
			return false;
		}
	}

	virtual const bool Stop() final
	{
		Task::disable();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return false;
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
			return false;
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		Task::enable();
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return 0;
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return 0;
	}

	virtual const uint16_t GetTimeToHop() final
	{
		return 0;
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	const uint8_t GetRealChannel(const uint8_t abstractChannel)
	{
		return (abstractChannel % ChannelCount);
	}
};
#endif