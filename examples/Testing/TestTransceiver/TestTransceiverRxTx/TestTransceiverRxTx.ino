/*
* TestTransceiverRxTx
* Simple sketch to test the Transceiver drivers.
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#endif

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define SERIAL_BAUD_RATE 2000000 // ESP Serial with low bitrate stalls the scheduler loop.
#else
#define SERIAL_BAUD_RATE 115200
#endif

#define _TASK_OO_CALLBACKS
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP32)
#define _TASK_THREAD_SAFE
#else
#define _TASK_SLEEP_ON_IDLE_RUN
#endif

// Selected Driver for test.
#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER2

#define LINK_DUPLEX_SLOT false

#include <TaskScheduler.h>
#include <ILoLaInclude.h>

#include "Testing\ExampleTransceiverDefinitions.h"
#include "Testing\ExampleLogicAnalyserDefinitions.h"

#include "TransmitReceiveTester.h"

// Process scheduler.
Scheduler SchedulerBase;
//

// Transceiver Driver.
#if defined(USE_SERIAL_TRANSCEIVER)
UartTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_BAUDRATE, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SERIAL_TRANSCEIVER_INSTANCE);
#elif defined(USE_NRF24_TRANSCEIVER)
SPIClass SpiTransceiver(NRF24_TRANSCEIVER_SPI_CHANNEL);
nRF24Transceiver<NRF24_TRANSCEIVER_PIN_CE, NRF24_TRANSCEIVER_PIN_CS, NRF24_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SpiTransceiver);
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
EspNowTransceiver TransceiverDriver(SchedulerBase);
#else
EspNowTransceiver TransceiverDriver(SchedulerBase);
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
Si446xTransceiver<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase);
#elif defined(USE_SI446X_TRANSCEIVER2)
SPIClass SpiTransceiver(SI446X_TRANSCEIVER_SPI_CHANNEL);
Si446xTransceiver2<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SpiTransceiver);
#else
#error No Transceiver.
#endif
//

// Test Task.
TransmitReceiveTester<true> TestTask(SchedulerBase, &TransceiverDriver);


void BootError()
{
#ifdef DEBUG
	Serial.println("Critical Error");
#endif  
	delay(1000);
	while (1);;
}

void setup()
{
#ifdef DEBUG
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
#endif

#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
	pinMode(SCHEDULER_TEST_PIN, OUTPUT);
#endif

#if defined(USE_SERIAL_TRANSCEIVER)
	TransceiverDriver.SetupInterrupt(OnSeriaInterrupt);
#elif defined(USE_NRF24_TRANSCEIVER)
	SpiTransceiver.begin();
	TransceiverDriver.SetupInterrupt(OnRxInterrupt);
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
	TransceiverDriver.SetupInterrupts(OnRxInterrupt);
#else
	TransceiverDriver.SetupInterrupts(OnRxInterrupt, OnTxInterrupt);
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
	TransceiverDriver.SetupInterrupt(OnRadioInterrupt);
#endif

	if (!TestTask.Setup())
	{
		Serial.println(F("Tester Task setup failed."));

		BootError();
	}

	Serial.println(F("TestTransceiver Tx Start!"));
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}

#if defined(USE_SERIAL_TRANSCEIVER)
void OnSeriaInterrupt()
{
	TransceiverDriver.OnSeriaInterrupt();
}
#elif defined(USE_NRF24_TRANSCEIVER)
void OnRxInterrupt()
{
	TransceiverDriver.OnRfInterrupt();
}
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
void OnRxInterrupt(const uint8_t* mac, const uint8_t* buf, size_t count, void* arg)
{
	TransceiverDriver.OnRxInterrupt(mac, buf, count);
}
#else
void OnRxInterrupt(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
	TransceiverDriver.OnRxInterrupt(data, data_len);
}
void OnTxInterrupt(const uint8_t* mac_addr, esp_now_send_status_t status)
{
	TransceiverDriver.OnTxInterrupt(status);
}
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
// Actual interrupt.
void OnRadioInterrupt()
{
	TransceiverDriver.OnRadioInterrupt();
}

// Global calls fired in event loop.
void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	TransceiverDriver.OnPostRxInterrupt(length, rssi);
}
void SI446X_CB_RXBEGIN(int16_t rssi)
{
	TransceiverDriver.OnPreRxInterrupt();
}
void SI446X_CB_SENT(void)
{
	TransceiverDriver.OnTxInterrupt();
}
#elif defined(USE_SI446X_TRANSCEIVER2)
void OnRadioInterrupt()
{
	TransceiverDriver.OnRadioInterrupt();
}
#endif