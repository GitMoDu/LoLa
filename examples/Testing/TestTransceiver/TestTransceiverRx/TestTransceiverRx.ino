/*
* TestTransceiverRx
* Simple sketch to test the Transceiver drivers.
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#define SERIAL_BAUD_RATE 115200
#endif

// Selected Transceiver for test.
#define USE_SI446X_TRANSCEIVER

#define LINK_DUPLEX_SLOT false

#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN
#include <TScheduler.hpp>

#include <ILoLaInclude.h>

#include "Testing\ExampleTransceiverDefinitions.h"
#include "Testing\ExampleLogicAnalyserDefinitions.h"

#include "ReceptionTester.h"

// Process scheduler.
TS::Scheduler SchedulerBase{};
//

// Transceiver.
#if defined(USE_SERIAL_TRANSCEIVER)
UartTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> Transceiver(SchedulerBase, &SERIAL_TRANSCEIVER_INSTANCE);
#elif defined(USE_NRF24_TRANSCEIVER)
nRF24Transceiver<NRF24_TRANSCEIVER_PIN_CE, NRF24_TRANSCEIVER_PIN_CS, NRF24_TRANSCEIVER_INTERRUPT_PIN, NRF24_TRANSCEIVER_SPI_CHANNEL> Transceiver(SchedulerBase);
#elif defined(USE_SI446X_TRANSCEIVER)
Si4463Transceiver_433_70_250<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN, UINT8_MAX, UINT8_MAX, UINT8_MAX, SI446X_TRANSCEIVER_SPI_CHANNEL> Transceiver(SchedulerBase);
#elif defined(USE_SX12_TRANSCEIVER)
Sx12Transceiver<SX12_TRANSCEIVER_PIN_CS, SX12_TRANSCEIVER_PIN_BUSY, SX12_TRANSCEIVER_PIN_RST, SX12_TRANSCEIVER_INTERRUPT_PIN, SX12_TRANSCEIVER_SPI_CHANNEL> Transceiver(SchedulerBase, &SpiTransceiver);
#elif defined(USE_ESPNOW_TRANSCEIVER) && defined(ARDUINO_ARCH_ESP32)
EspNowTransceiver Transceiver(SchedulerBase);
#else
#error No Transceiver.
#endif
//

// Test Task.
ReceptionTester TestTask(&Transceiver);

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

#if defined(USE_SERIAL_TRANSCEIVER) || defined(USE_SI446X_TRANSCEIVER) || defined(USE_SX12_TRANSCEIVER) || defined(USE_NRF24_TRANSCEIVER)
	Transceiver.SetupInterrupt(OnInterrupt);
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
	Transceiver.SetupInterrupts(OnRxInterrupt);
#else
	Transceiver.SetupInterrupts(OnRxInterrupt, OnTxInterrupt);
#endif
#endif

	if (!TestTask.Setup())
	{
		Serial.println(F("Tester Task setup failed."));

		BootError();
	}

	Serial.println(F("TestTransceiver Rx Start!"));
	pinMode(RX_TEST_PIN, OUTPUT);
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}

#if defined(USE_SERIAL_TRANSCEIVER) || defined(USE_NRF24_TRANSCEIVER) || defined(USE_SI446X_TRANSCEIVER) || defined(USE_SX12_TRANSCEIVER)
void OnInterrupt()
{
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, HIGH);
#endif
	Transceiver.OnInterrupt();
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, LOW);
#endif
}
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
void OnRxInterrupt(const uint8_t* mac, const uint8_t* buf, size_t count, void* arg)
{
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, HIGH);
#endif
	Transceiver.OnRxInterrupt(mac, buf, count);
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, LOW);
#endif
}
#else
void OnRxInterrupt(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, HIGH);
#endif
	Transceiver.OnRxInterrupt(data, data_len);
#if defined(RX_TEST_PIN)
	digitalWrite(RX_TEST_PIN, LOW);
#endif
}
void OnTxInterrupt(const uint8_t* mac_addr, esp_now_send_status_t status)
{
	Transceiver.OnTxInterrupt(status);
}
#endif
#endif