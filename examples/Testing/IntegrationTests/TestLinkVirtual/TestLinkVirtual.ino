/* LoLa Link Virtual tester.
* Creates instances of both host and receiver,
* communicating through a virtual radio.
*
* Used to testing and developing the LoLa Link protocol.
*
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

#define HOP_TEST_PIN TEST_PIN_3
#define HOP_TEST_PIN_2 TEST_PIN_4

#define TX_HOST_TEST_PIN TEST_PIN_1
#define TX_REMOTE_TEST_PIN TEST_PIN_2
#else
#define HOP_TEST_PIN 0
#define HOP_TEST_PIN_2 0
#define TX_HOST_TEST_PIN 0
#define TX_REMOTE_TEST_PIN 0
#endif




// Medium Simulation error chances, out 255, for every call.
//#define PRINT_PACKETS
//#define DROP_CHANCE 10
//#define CORRUPT_CHANCE 10
//#define ECO_CHANCE 5
//#define DOUBLE_SEND_CHANCE 5

#define HOST_DROP_LINK_TEST 5
//#define REMOTE_DROP_LINK_TEST 5
//#define PRINT_TEST_PACKETS false

#define PRINT_CHANNEL_HOP false
#define PRINT_LINK_HEARBEAT 1

#define LINK_USE_CHANNEL_HOP true
//#define LINK_USE_TIMER_AND_RTC

#define _TASK_SCHEDULE_NC // Disable task catch-up.
#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>

#include <ILoLaInclude.h>

#include "TestTask.h"

// Process scheduler.
Scheduler SchedulerBase;
//


// Diceware created access control password.
static const uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Created offline using PublicPrivateKeyGenerator example project.
static const uint8_t HostPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static const uint8_t HostPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };

static const uint8_t RemotePublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x21,0x9B,0xA6,0x2A,0xF0,0x14,0x8A,0x62,0xEB,0x2A,0xF0,0xD1,0xAC,0xB4,0x6E,0x15,0x1B,0x63,0xA7,0xEA,0x99,0xFD,0x1E,0xDD,0x74,0x2F,0xD4,0xB0,0xE1,0x04,0xC5,0x96,0x09,0x65,0x1F,0xAB,0x4F,0xDC,0x75,0x0C };
static const uint8_t RemotePrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x9E,0x78,0xBA,0x67,0xEA,0x57,0xA9,0xBD,0x4E,0x1A,0x35,0xFB,0xD3,0xA7,0x19,0x29,0x55,0xB9,0xA1,0x3A };
//



// Test Virtual Packet Driver configurations.
using FastGHzRadio = IVirtualPacketDriver::Configuration<100, 25, 100, 10, 50, 10>;
using TypicalSubGHzRadio = IVirtualPacketDriver::Configuration<150, 50, 250, 20, 250, 20>;
using SlowSingleChannelRadio = IVirtualPacketDriver::Configuration<1, 100, 500, 50, 400, 50>;
using IdealRadio = IVirtualPacketDriver::Configuration<1, 1, 1, 1, 1, 1>;
// Used Virtual Driver Configuration.
using TestRadioConfig = TypicalSubGHzRadio;
//

// Shared Link configuration.
static const uint16_t DuplexPeriod = 10000;
static const uint32_t ChannelHopPeriod = 20000;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource EntropySource;
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266EntropySource EntropySource;
#else 
ArduinoEntropySource EntropySource;
#endif



#if !defined(LINK_USE_TIMER_AND_RTC)
ArduinoTaskTimerClockSource<> SharedTimerClockSource(SchedulerBase);
IClockSource* HostClock = &SharedTimerClockSource;
ITimerSource* HostTimer = &SharedTimerClockSource;
IClockSource* RemoteClock = &SharedTimerClockSource;
ITimerSource* RemoteTimer = &SharedTimerClockSource;
#else
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32TimerSource HostTimerSource(1, 'A');
Stm32RtcClockSource HostClockSource(SchedulerBase);

Stm32RtcClockSource RemoteClockSource(SchedulerBase);
Stm32TimerSource RemoteTimerSource(2, 'A');
//#elif defined(ARDUINO_ARCH_ESP8266)
#else 
#error No RTC/Timer sources found.
#endif
IClockSource* HostClock = &HostClockSource;
ITimerSource* HostTimer = &HostTimerSource;

IClockSource* RemoteClock = &RemoteClockSource;
ITimerSource* RemoteTimer = &RemoteTimerSource;
#endif

#if defined(LINK_USE_CHANNEL_HOP)
TimedChannelHopper<ChannelHopPeriod> HostChannelHop(SchedulerBase);
TimedChannelHopper<ChannelHopPeriod, HOP_TEST_PIN, HOP_TEST_PIN_2> RemoteChannelHop(SchedulerBase);
#else
NoHopNoChannel HostChannelHop;
NoHopNoChannel RemoteChannelHop;
#endif


// Link host and its required instances.
VirtualHalfDuplexDriver<TestRadioConfig, 'H', false, TX_HOST_TEST_PIN> HostDriver(SchedulerBase);
HalfDuplex<DuplexPeriod, false> HostDuplex;
LoLaLinkHost<> Host(SchedulerBase,
	&HostDriver,
	&EntropySource,
	HostClock,
	HostTimer,
	&HostDuplex,
	&HostChannelHop,
	HostPublicKey,
	HostPrivateKey,
	Password);

// Link Remote.
VirtualHalfDuplexDriver<TestRadioConfig, 'R', PRINT_CHANNEL_HOP, TX_REMOTE_TEST_PIN> RemoteDriver(SchedulerBase);
HalfDuplex<DuplexPeriod, true> RemoteDuplex;
LoLaLinkRemote<> Remote(SchedulerBase,
	&RemoteDriver,
	&EntropySource,
	RemoteClock,
	RemoteTimer,
	&RemoteDuplex,
	&RemoteChannelHop,
	RemotePublicKey,
	RemotePrivateKey,
	Password);

TestTask Tester(SchedulerBase, &Host, &Remote);


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

	// Setup Virtual Packet Drivers.
	HostDriver.SetPartner(&RemoteDriver);
	RemoteDriver.SetPartner(&HostDriver);

	// Setup Test Task.
	if (!Tester.Setup())
	{
		Serial.println(F("Tester Task setup failed."));

		BootError();
	}

	// Setup Link instances.
	if (!Host.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Host Link Setup Failed."));
#endif
		BootError();
	}
	if (!Remote.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Remote Link Setup Failed."));
#endif
		BootError();
	}

	// Start Link instances.
	if (Host.Start() &&
		Remote.Start())
	{
		Serial.print(millis());
		Serial.println(F("\tLoLa Links have started."));
	}
	else
	{
#ifdef DEBUG
		Serial.println(F("Link Start Failed."));
#endif
		BootError();
	}
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
#endif
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}
