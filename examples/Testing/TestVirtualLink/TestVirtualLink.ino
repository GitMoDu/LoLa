/* LoLa Link Virtual tester.
* Creates instances of both Server and Client,
* communicating through a virtual radio.
*
* Used to testing and developing the LoLa Link protocol.
*
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
#endif

#if defined(ARDUINO_ARCH_ESP32)
// ESP32 Serial with low bitrate stalls the scheduler loop.
#define SERIAL_BAUD_RATE 1000000
#else
#define SERIAL_BAUD_RATE 115200
#endif

#if !defined(LED_BUILTIN) && defined(ARDUINO_ARCH_ESP32)
#define LED_BUILTIN 33
#endif // !LED_BUILTIN


// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 15
#define TEST_PIN_1 16
#define TEST_PIN_2 17
#define TEST_PIN_3 18
//#define TEST_PIN_4 19

#define SCHEDULER_TEST_PIN TEST_PIN_0

#define HOP_TEST_PIN TEST_PIN_3

#define TX_SERVER_TEST_PIN TEST_PIN_1
#define TX_CLIENT_TEST_PIN TEST_PIN_2
#else
#define HOP_TEST_PIN 0
#define TX_SERVER_TEST_PIN 0
#define TX_CLIENT_TEST_PIN 0
#endif




// Medium Simulation error chances, out 255, for every call.
//#define DROP_CHANCE 70
//#define CORRUPT_CHANCE 20
//#define DOUBLE_SEND_CHANCE 10
//#define ECO_CHANCE 5

//#define SERVER_DROP_LINK_TEST 5
//#define CLIENT_DROP_LINK_TEST 5

//#define PRINT_PACKETS
//#define PRINT_TEST_PACKETS
#define PRINT_DISCOVERY

#define PRINT_CHANNEL_HOP false
#define PRINT_LINK_HEARBEAT 1

#define LINK_USE_CHANNEL_HOP true
//#define LINK_USE_TIMER_AND_RTC

#define _TASK_OO_CALLBACKS
#ifdef _TASK_SLEEP_ON_IDLE_RUN
#undef _TASK_SLEEP_ON_IDLE_RUN // Virtual Transceiver can't wake up the CPU, sleep is not compatible.
#endif

#include <TaskScheduler.h>

#include <ILoLaInclude.h>

#include "TestTask.h"

#include "DiscoveryTestService.h"

// Process scheduler.
Scheduler SchedulerBase;
//


// Diceware created access control password.
static const uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Created offline using PublicPrivateKeyGenerator example project.
static const uint8_t ServerPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static const uint8_t ServerPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };

static const uint8_t ClientPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x21,0x9B,0xA6,0x2A,0xF0,0x14,0x8A,0x62,0xEB,0x2A,0xF0,0xD1,0xAC,0xB4,0x6E,0x15,0x1B,0x63,0xA7,0xEA,0x99,0xFD,0x1E,0xDD,0x74,0x2F,0xD4,0xB0,0xE1,0x04,0xC5,0x96,0x09,0x65,0x1F,0xAB,0x4F,0xDC,0x75,0x0C };
static const uint8_t ClientPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x9E,0x78,0xBA,0x67,0xEA,0x57,0xA9,0xBD,0x4E,0x1A,0x35,0xFB,0xD3,0xA7,0x19,0x29,0x55,0xB9,0xA1,0x3A };
//

// Virtual Transceiver configurations.
// <ChannelCount, TxBaseMicros, TxByteNanos, AirBaseMicros, AirByteNanos, HopMicros>
using SlowSingleChannel = IVirtualTransceiver::Configuration<1, 160, 1000, 200, 20000, 50>;
using FastSingleChannel = IVirtualTransceiver::Configuration<1, 40, 500, 40, 2000, 10>;
using SlowMultiChannel = IVirtualTransceiver::Configuration<10, 160, 1000, 200, 20000, 50>;
using FastMultiChannel = IVirtualTransceiver::Configuration<160, 40, 500, 40, 2000, 10>;

// Used Virtual Driver Configuration.
using TestRadioConfig = FastMultiChannel;

// Shared Link configuration.
static const uint16_t DuplexPeriod = 5000;
static const uint16_t DuplexDeadZone = DuplexPeriod / 20;
static const uint32_t ChannelHopPeriod = DuplexPeriod;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32Entropy EntropySource{};
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266Entropy EntropySource{};
#else 
ArduinoEntropy EntropySource{};
#endif

#if !defined(LINK_USE_TIMER_AND_RTC)
ArduinoCycles ServerCyclesSource{};
ArduinoCycles ClientCyclesSource{};
#else
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32TimerCycles<1, 'A'> ServerCyclesSource{};
Stm32TimerCycles<2, 'A'> ClientCyclesSource{};
#elif defined(ARDUINO_ARCH_ESP8266)
#error No CyclesSource found.
#else 
#error No CyclesSource found.
#endif
#endif

#if defined(LINK_USE_CHANNEL_HOP)
TimedChannelHopper<ChannelHopPeriod> ServerChannelHop(SchedulerBase);
TimedChannelHopper<ChannelHopPeriod> ClientChannelHop(SchedulerBase);
#else
NoHopNoChannel ServerChannelHop{};
NoHopNoChannel ClientChannelHop{};
#endif

// Link Server and its required instances.
VirtualTransceiver<TestRadioConfig, 'S', false, TX_SERVER_TEST_PIN> ServerTransceiver(SchedulerBase);
HalfDuplex<DuplexPeriod, false, DuplexDeadZone> ServerDuplex;
LoLaPkeLinkServer<> Server(SchedulerBase,
	&ServerTransceiver,
	&ServerCyclesSource,
	&EntropySource,
	&ServerDuplex,
	&ServerChannelHop,
	ServerPublicKey,
	ServerPrivateKey,
	Password);

// Link Client and its required instances.
VirtualTransceiver<TestRadioConfig, 'C', PRINT_CHANNEL_HOP, TX_CLIENT_TEST_PIN> ClientTransceiver(SchedulerBase);
HalfDuplex<DuplexPeriod, true, DuplexDeadZone> ClientDuplex;
LoLaPkeLinkClient<> Client(SchedulerBase,
	&ClientTransceiver,
	&ClientCyclesSource,
	&EntropySource,
	&ClientDuplex,
	&ClientChannelHop,
	ClientPublicKey,
	ClientPrivateKey,
	Password);

TestTask Tester(SchedulerBase, &Server, &Client);

DiscoveryTestService<'S', 0, 12345> ServerDiscovery(SchedulerBase, &Server);
DiscoveryTestService<'C', 0, 12345> ClientDiscovery(SchedulerBase, &Client);


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

	// Setup Virtual Packet Drivers.
	ServerTransceiver.SetPartner(&ClientTransceiver);
	ClientTransceiver.SetPartner(&ServerTransceiver);

	// Setup Test Task.
	if (!Tester.Setup())
	{
		Serial.println(F("Tester Task setup failed."));

		BootError();
	}

	// Setup Test Discovery services.
	if (!ServerDiscovery.Setup())
	{
		Serial.println(F("ServerDiscovery setup failed."));

		BootError();
	}
	if (!ClientDiscovery.Setup())
	{
		Serial.println(F("ClientDiscovery setup failed."));

		BootError();
	}

	// Setup Link instances.
	if (!Server.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Server Link Setup Failed."));
#endif
		BootError();
	}
	if (!Client.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Client Link Setup Failed."));
#endif
		BootError();
	}

	// Start Link instances.
	if (Server.Start() &&
		Client.Start())
	{
		Serial.print(micros());
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
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}
