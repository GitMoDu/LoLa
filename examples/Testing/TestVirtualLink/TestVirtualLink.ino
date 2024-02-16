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
#define SERIAL_BAUD_RATE 115200
#endif

#define _TASK_OO_CALLBACKS
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define _TASK_THREAD_SAFE	// Enable additional checking for thread safety
#endif
#ifdef _TASK_SLEEP_ON_IDLE_RUN
#undef _TASK_SLEEP_ON_IDLE_RUN // Virtual Transceiver can't wake up the CPU, sleep is not compatible.
#endif

// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 17
#define TEST_PIN_1 18
#define TEST_PIN_2 19

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define TX_SERVER_TEST_PIN TEST_PIN_1
#define TX_CLIENT_TEST_PIN TEST_PIN_2
#endif

// Enable for Public-Key Exchange Link. Disable for Address Match Link.
//#define LINK_USE_PKE_LINK
//#define LOLA_USE_POLY1305

// Enable to use channel hop. Disable for fixed channel.
#define LINK_USE_CHANNEL_HOP

// Client clock detune in us. Disable for no detune.
#define LINK_TEST_DETUNE 5

// Log the last error, deviation and tune.
#define LOLA_DEBUG_LINK_CLOCK

// Enable to log raw packets in transit.
//#define PRINT_PACKETS

// Medium Simulation error chances, out 255, for every call.
//#define DROP_CHANCE 70
//#define CORRUPT_CHANCE 20
//#define DOUBLE_SEND_CHANCE 10
//#define ECO_CHANCE 5

// DROP_LINK_TEST period in ms. Disable for no test link drop.
//#define SERVER_DROP_LINK_TEST 5
//#define CLIENT_DROP_LINK_TEST 5

// Enable to log the test packets being sent/received.
//#define PRINT_TEST_PACKETS

// Period in seconds, to log the Links' status. Disable for no heartbeat.
#define PRINT_LINK_HEARBEAT 5

// Set to true to log the current Client channel, as it hops. False to disable.
#define PRINT_CHANNEL_HOP false


#include <TaskScheduler.h>
#include <ILoLaInclude.h>

#include "TestTask.h"
#include "ClockDetunerTask.h"
#include "DiscoveryTestService.h"

// Process scheduler.
Scheduler SchedulerBase;
//

// Diceware created access control password.
static const uint8_t AccessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x10, 0x01, 0x20, 0x02, 0x30, 0x03, 0x40, 0x04 };
//

#if defined(LINK_USE_PKE_LINK)
// Created offline using PublicPrivateKeyGenerator example project.
static const uint8_t ServerPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static const uint8_t ServerPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };

static const uint8_t ClientPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x21,0x9B,0xA6,0x2A,0xF0,0x14,0x8A,0x62,0xEB,0x2A,0xF0,0xD1,0xAC,0xB4,0x6E,0x15,0x1B,0x63,0xA7,0xEA,0x99,0xFD,0x1E,0xDD,0x74,0x2F,0xD4,0xB0,0xE1,0x04,0xC5,0x96,0x09,0x65,0x1F,0xAB,0x4F,0xDC,0x75,0x0C };
static const uint8_t ClientPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x9E,0x78,0xBA,0x67,0xEA,0x57,0xA9,0xBD,0x4E,0x1A,0x35,0xFB,0xD3,0xA7,0x19,0x29,0x55,0xB9,0xA1,0x3A };
#else
// Diceware created values for address and secret key.
static const uint8_t ServerAddress[LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
static const uint8_t ClientAddress[LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
static const uint8_t SecretKey[LoLaLinkDefinition::SECRET_KEY_SIZE] = { 0x50, 0x05, 0x60, 0x06, 0x70, 0x07, 0x80, 0x08 };
#endif


// Virtual Transceiver configurations.
// <ChannelCount, TxBaseMicros, TxByteNanos, AirBaseMicros, AirByteNanos, HopMicros>
using SlowSingleChannel = IVirtualTransceiver::Configuration<1, 50, 2000, 500, 20000, 50>;
using FastSingleChannel = IVirtualTransceiver::Configuration<1, 5, 500, 40, 2000, 10>;
using SlowMultiChannel = IVirtualTransceiver::Configuration<10, 50, 2000, 500, 20000, 50>;
using FastMultiChannel = IVirtualTransceiver::Configuration<160, 5, 500, 40, 2000, 10>;

// Used Virtual Driver Configuration.
using TestRadioConfig = SlowMultiChannel;

// Shared Link configuration.
static const uint16_t DuplexPeriod = 5000;
static const uint16_t DuplexDeadZone = 200 + TestRadioConfig::HopMicros;
static const uint32_t ChannelHopPeriod = DuplexPeriod / 2;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32Entropy ServerEntropySource{};
Stm32Entropy ClientEntropySource{};
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266Entropy ServerEntropySource{};
Esp8266Entropy ClientEntropySource{};
#else 
ArduinoLowEntropy ServerEntropySource{};
ArduinoLowEntropy ClientEntropySource{};
#endif

#if defined(LINK_USE_CHANNEL_HOP)
TimedChannelHopper<ChannelHopPeriod> ServerChannelHop(SchedulerBase);
TimedChannelHopper<ChannelHopPeriod> ClientChannelHop(SchedulerBase);
#else
NoHopNoChannel ServerChannelHop{};
NoHopNoChannel ClientChannelHop{};
#endif

// Link Server and its required instances.
#ifdef TX_SERVER_TEST_PIN
VirtualTransceiver<TestRadioConfig, 'S', false, TX_SERVER_TEST_PIN> ServerTransceiver(SchedulerBase);
#else
VirtualTransceiver<TestRadioConfig, 'S', false> ServerTransceiver(SchedulerBase);
#endif

HalfDuplex<DuplexPeriod, false, DuplexDeadZone> ServerDuplex;
ArduinoCycles ServerCyclesSource{};
#if defined(LINK_USE_PKE_LINK)
LoLaPkeLinkServer<> Server(SchedulerBase,
	&ServerTransceiver,
	&ServerCyclesSource,
	&ServerEntropySource,
	&ServerDuplex,
	&ServerChannelHop,
	AccessPassword,
	ServerPublicKey,
	ServerPrivateKey);
#else
LoLaAddressMatchLinkServer<> Server(SchedulerBase,
	&ServerTransceiver,
	&ServerCyclesSource,
	&ServerEntropySource,
	&ServerDuplex,
	&ServerChannelHop,
	SecretKey,
	AccessPassword,
	ServerAddress);
#endif

// Link Client and its required instances.
#ifdef TX_CLIENT_TEST_PIN
VirtualTransceiver<TestRadioConfig, 'C', PRINT_CHANNEL_HOP, TX_CLIENT_TEST_PIN> ClientTransceiver(SchedulerBase);
#else
VirtualTransceiver<TestRadioConfig, 'C', PRINT_CHANNEL_HOP> ClientTransceiver(SchedulerBase);
#endif

HalfDuplex<DuplexPeriod, true, DuplexDeadZone> ClientDuplex;
ArduinoCycles ClientCyclesSource{};
#if defined(LINK_USE_PKE_LINK)
LoLaPkeLinkClient<> Client(SchedulerBase,
	&ClientTransceiver,
	&ClientCyclesSource,
	&ClientEntropySource,
	&ClientDuplex,
	&ClientChannelHop,
	AccessPassword,
	ClientPublicKey,
	ClientPrivateKey);
#else
LoLaAddressMatchLinkClient<> Client(SchedulerBase,
	&ClientTransceiver,
	&ClientCyclesSource,
	&ClientEntropySource,
	&ClientDuplex,
	&ClientChannelHop,
	SecretKey,
	AccessPassword,
	ClientAddress);
#endif

#if defined(LINK_TEST_DETUNE)
ClockDetunerTask Detuner(SchedulerBase);
#endif

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
	pinMode(SCHEDULER_TEST_PIN, OUTPUT);
#endif

	// Setup Virtual Packet Drivers.
	ServerTransceiver.SetPartner(&ClientTransceiver);
	ClientTransceiver.SetPartner(&ServerTransceiver);

	// Setup Test Task.
	if (!Tester.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Tester Task setup failed."));
#endif
		BootError();
	}

	// Setup Test Discovery services.
	if (!ServerDiscovery.Setup())
	{
#ifdef DEBUG
		Serial.println(F("ServerDiscovery setup failed."));
#endif
		BootError();
	}
	if (!ClientDiscovery.Setup())
	{
#ifdef DEBUG
		Serial.println(F("ClientDiscovery setup failed."));
#endif
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

#if defined(LINK_TEST_DETUNE)
	// Detune client clock by LINK_TEST_DETUNE us every second.
	Detuner.SetClockDetune(Client.GetInternalClock(), -LINK_TEST_DETUNE);
#endif

	// Start Link instances.
	if (Server.Start() &&
		Client.Start())
	{
#ifdef DEBUG_LOLA
		Serial.print(millis());
		Serial.println(F("\tLoLa Links have started."));
#endif
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
