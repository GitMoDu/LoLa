/*
* TestTransceiverSend
* Simple sketch to test the Transceiver drivers.
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#endif

#define SERIAL_BAUD_RATE 115200

#if !defined(LED_BUILTIN) && defined(ARDUINO_ARCH_ESP32)
#define LED_BUILTIN 33
#endif // !LED_BUILTIN

// Selected Driver for test.
#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF21_TRANSCEIVER

// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 15
#define TEST_PIN_1 16
#define TEST_PIN_2 17

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define RX_TEST_PIN TEST_PIN_1
#define TX_TEST_PIN TEST_PIN_2
#endif


#define _TASK_SCHEDULE_NC // Disable task catch-up.
#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>
#include <ILoLaInclude.h>
#include "TransmitReceiveTester.h"

// Transceiver Definitions.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 10
#define SERIAL_TRANSCEIVER_INSTANCE			Serial2
#define SERIAL_TRANSCEIVER_BAUDRATE			500000
#elif defined(USE_NRF21_TRANSCEIVER)
#define NRF21_TRANSCEIVER_PIN_CE			3
#define NRF21_TRANSCEIVER_PIN_CS			7
#define NRF21_TRANSCEIVER_RX_INTERRUPT_PIN	1
#define NRF21_TRANSCEIVER_DATA_RATE			RF24_1MBPS
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			0
#define SI446X_TRANSCEIVER_PIN_SDN			12
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	11
#endif
#else
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 2
#define SERIAL_TRANSCEIVER_INSTANCE			Serial
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF21_TRANSCEIVER)
#define NRF21_TRANSCEIVER_PIN_CS			10
#define NRF21_TRANSCEIVER_PIN_CE			9
#define NRF21_TRANSCEIVER_RX_INTERRUPT_PIN	2
#define NRF21_TRANSCEIVER_DATA_RATE			RF24_250KBPS
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
SerialTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_BAUDRATE, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SERIAL_TRANSCEIVER_INSTANCE);
#elif defined(USE_NRF21_TRANSCEIVER)
nRF24Transceiver<NRF21_TRANSCEIVER_PIN_CE, NRF21_TRANSCEIVER_PIN_CS, NRF21_TRANSCEIVER_RX_INTERRUPT_PIN, NRF21_TRANSCEIVER_DATA_RATE> TransceiverDriver(SchedulerBase);
#endif
//

// Test Task.
TransmitReceiveTester TestTask(SchedulerBase, &TransceiverDriver);


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
#endif
#if defined(USE_NRF21_TRANSCEIVER)
	TransceiverDriver.SetupInterrupt(OnRxInterrupt);
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
#endif
#if defined(USE_NRF21_TRANSCEIVER)
void OnRxInterrupt()
{
	TransceiverDriver.OnRfInterrupt();
}
#endif