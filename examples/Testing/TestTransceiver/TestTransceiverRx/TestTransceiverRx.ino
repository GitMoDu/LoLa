/*
* TestTransceiverReceive
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

#if !defined(LED_BUILTIN) && defined(ARDUINO_ARCH_ESP32)
#define LED_BUILTIN 33
#endif // !LED_BUILTIN

// Selected Driver for test.
#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER


// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 15
#define TEST_PIN_1 16
#define TEST_PIN_2 17

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define RX_TEST_PIN TEST_PIN_1
#define RX_INTERRUPT_TEST_PIN TEST_PIN_2
#endif


#define _TASK_OO_CALLBACKS
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP32)
#define _TASK_THREAD_SAFE
#else
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>
#include <ILoLaInclude.h>
#include "ReceptionTester.h"

// Transceiver Definitions.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 8
#define SERIAL_TRANSCEIVER_INSTANCE			Serial2
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_PIN_CE			3
#define NRF24_TRANSCEIVER_PIN_CS			7
#define NRF24_TRANSCEIVER_RX_INTERRUPT_PIN	1
#define NRF24_TRANSCEIVER_DATA_RATE			RF24_250KBPS
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
#elif defined(ARDUINO_ARCH_ESP32)
#define ESPNOW_TRANSCEIVER_DATA_RATE		WIFI_PHY_RATE_1M_L
#else
#error "USE_ESPNOW_TRANSCEIVER Is only available for the ESP32 and ESP8266 Arduino core platforms."
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			31
#define SI446X_TRANSCEIVER_PIN_SDN			25
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	26
#endif
#else
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 2
#define SERIAL_TRANSCEIVER_INSTANCE			Serial
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_PIN_CS			10
#define NRF24_TRANSCEIVER_PIN_CE			9
#define NRF24_TRANSCEIVER_RX_INTERRUPT_PIN	2
#define NRF24_TRANSCEIVER_DATA_RATE			RF24_250KBPS
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			10
#define SI446X_TRANSCEIVER_PIN_SDN			9
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	2
#endif
#endif
//

// Process scheduler.
Scheduler SchedulerBase;
//

// Transceiver Driver.
#if defined(USE_SERIAL_TRANSCEIVER)
UartTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_BAUDRATE, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SERIAL_TRANSCEIVER_INSTANCE);
#elif defined(USE_NRF24_TRANSCEIVER)
nRF24Transceiver<NRF24_TRANSCEIVER_PIN_CE, NRF24_TRANSCEIVER_PIN_CS, NRF24_TRANSCEIVER_RX_INTERRUPT_PIN, NRF24_TRANSCEIVER_DATA_RATE> TransceiverDriver(SchedulerBase);
#elif defined(USE_ESPNOW_TRANSCEIVER)
EspNowTransceiver<ESPNOW_TRANSCEIVER_DATA_RATE> TransceiverDriver(SchedulerBase);
#elif defined(USE_SI446X_TRANSCEIVER)
Si446xTransceiver<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase);
#endif
//

// Test Task.
ReceptionTester TestTask(&TransceiverDriver);

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

#if (RX_INTERRUPT_TEST_PIN >0)
	digitalWrite(RX_INTERRUPT_TEST_PIN, LOW);
	pinMode(RX_INTERRUPT_TEST_PIN, OUTPUT);
#endif

#if defined(USE_SERIAL_TRANSCEIVER)
	TransceiverDriver.SetupInterrupt(OnSeriaInterrupt);
#elif defined(USE_NRF24_TRANSCEIVER)
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

	Serial.println(F("TestTransceiver Rx Start!"));
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
void OnRadioInterrupt()
{
	TransceiverDriver.OnRadioInterrupt();
}

void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	TransceiverDriver.OnPostRxInterrupt(length, rssi);
}

void SI446X_CB_RXINVALID(int16_t rssi)
{
	TransceiverDriver.OnPostRxInterrupt(0, rssi);
}

void SI446X_CB_RXBEGIN(int16_t rssi)
{
	TransceiverDriver.OnPreRxInterrupt();
}

void SI446X_CB_SENT(void)
{
	TransceiverDriver.OnTxInterrupt();
}
#endif