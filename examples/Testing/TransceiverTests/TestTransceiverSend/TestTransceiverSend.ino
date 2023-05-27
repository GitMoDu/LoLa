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


// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 15
#define TEST_PIN_1 16
#define TEST_PIN_2 17
#define TEST_PIN_3 18
#define TEST_PIN_4 19
#define TEST_PIN_5 20
#define TEST_PIN_6 6
#define TEST_PIN_7 7

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define TEST_TX_START_PIN TEST_PIN_1
#else
#define TEST_TX_START_PIN 0
#endif


#define _TASK_SCHEDULE_NC // Disable task catch-up.
#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>
#include <ILoLaInclude.h>
#include "TransmissionTester.h"



// Process scheduler.
Scheduler SchedulerBase;
//

// Transceiver Drivers.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
SerialTransceiver<HardwareSerial, 115200, 21> SerialDriver(SchedulerBase, &Serial3);
#else
SerialTransceiver<HardwareSerial, 115200, 21> SerialDriver(SchedulerBase, &Serial);
#endif

VirtualTransceiver<IVirtualTransceiver::Configuration<1, 1, 1, 1, 1, 1>, 'T'> VirtualDriver(SchedulerBase);

// Selected Driver for test.
ILoLaTransceiver* TestTransceiver = &VirtualDriver;

// Test Task.
static const uint32_t SendPeriodMillis = 10;
TransmissionTester<SendPeriodMillis, TEST_TX_START_PIN> TestTask(SchedulerBase, TestTransceiver);

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

	if (!TestTask.Setup())
	{
		Serial.println(F("Tester Task setup failed."));

		BootError();
	}


	Serial.println(F("TestTransceiver Send Start!"));
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}
