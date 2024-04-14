/* LoLa Link Virtual tester.
* Creates instances of both Server and Client,
* communicating through a pair of virtual transceivers.
*
* Used to testing and developing the LoLa Link protocol.
*
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
//#define DEBUG_LOLA_LINK
#define SERIAL_BAUD_RATE 115200
#endif

#define _TASK_OO_CALLBACKS
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

// Enable for experimental faster hash.
//#define LOLA_USE_POLY1305

// Enable to use channel hop. Disable for fixed channel.
#define LINK_USE_CHANNEL_HOP

// Client clock detune in us. Disable for no detune.
#define LINK_TEST_DETUNE 5

// Log the last error, deviation and tune.
//#define LOLA_DEBUG_LINK_CLOCK

// Test Discovery Service.
#define LINK_TEST_DISCOVERY

// Test Surface Service.
//#define LINK_TEST_SURFACE

// Enable to log raw packets in transit.
//#define PRINT_PACKETS

// Medium Simulation error chances, out 255, for every call.
//#define DROP_CHANCE 50
//#define CORRUPT_CHANCE 30
//#define DOUBLE_SEND_CHANCE 10
//#define ECO_CHANCE 5

// DROP_LINK_TEST period in ms. Disable for no test link drop.
//#define SERVER_DROP_LINK_TEST 5
//#define CLIENT_DROP_LINK_TEST 5

// Period in seconds, to log the Links' status. Disable for no heartbeat.
//#define PRINT_LINK_HEARBEAT 5

// Set to true to log the current Client channel, as it hops. False to disable.
//#define PRINT_CHANNEL_HOP true

// Enable to use Controller to write to ExampleSurface. 
// Depends on https://github.com/GitMoDu/NintendoControllerReader
//#define USE_N64_CONTROLLER
//#define USE_GAMECUBE_CONTROLLER

// Enable to use ArduinoGraphicsEngine for displaying live link info.
// Depends on https://github.com/GitMoDu/ArduinoGraphicsEngine
//#define USE_LINK_DISPLAY
//#define GRAPHICS_ENGINE_MEASURE


#include <TaskScheduler.h>
#include <ILoLaInclude.h>

#include "TestTask.h"

#if defined(LINK_TEST_DETUNE)
#include "../src/Testing/ClockDetunerTask.h"
#endif
#if defined(LINK_TEST_DISCOVERY)
#include "../src/Testing/DiscoveryTestService.h"
#endif
#if defined(LINK_TEST_SURFACE)
#include "../src/Testing/ExampleSurface.h"
#if defined(USE_N64_CONTROLLER) || defined(USE_GAMECUBE_CONTROLLER)
#define USE_CONTROLLER 
#include "../src/Testing/ControllerSource.h"
#include "../src/Testing/ControllerExampleSurfaceDispatcher.h"
#endif
#endif

#if defined(USE_LINK_DISPLAY)
#include "VirtualLinkGraphicsEngine.h"
#endif


// Process scheduler.
Scheduler SchedulerBase;
//

// Diceware created access control password.
static const uint8_t AccessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x10, 0x01, 0x20, 0x02, 0x30, 0x03, 0x40, 0x04 };
//

// Diceware created values for address and secret key.
static const uint8_t ServerAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
static const uint8_t ClientAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
static const uint8_t SecretKey[LoLaLinkDefinition::SECRET_KEY_SIZE] = { 0x50, 0x05, 0x60, 0x06, 0x70, 0x07, 0x80, 0x08 };

// Virtual Transceiver configurations.
// <ChannelCount, TxBaseMicros, TxByteNanos, AirBaseMicros, AirByteNanos, HopMicros>
using SlowSingleChannel = IVirtualTransceiver::Configuration<1, 50, 4000, 700, 35000, 100>;
using SlowMultiChannel = IVirtualTransceiver::Configuration<10, 50, 4000, 700, 35000, 100>;
using FastSingleChannel = IVirtualTransceiver::Configuration<1, 10, 500, 50, 7500, 20>;
using FastMultiChannel = IVirtualTransceiver::Configuration<160, 10, 500, 50, 7500, 20>;

// Used Virtual Driver Configuration.
using TestRadioConfig = SlowMultiChannel;

// Shared Link configuration.
static const uint16_t DuplexPeriod = 10000;
static const uint16_t DuplexDeadZone = 500;
static const uint32_t ChannelHopPeriod = 200000;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1)
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
TimedChannelHopper<ChannelHopPeriod, DuplexDeadZone> ServerChannelHop(SchedulerBase);
TimedChannelHopper<ChannelHopPeriod, DuplexDeadZone> ClientChannelHop(SchedulerBase);
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
LoLaAddressMatchLinkServer<> Server(SchedulerBase,
	&ServerTransceiver,
	&ServerCyclesSource,
	&ServerEntropySource,
	&ServerDuplex,
	&ServerChannelHop,
	SecretKey,
	AccessPassword,
	ServerAddress);

// Link Client and its required instances.
#ifndef PRINT_CHANNEL_HOP
#define PRINT_CHANNEL_HOP false
#endif
#ifdef TX_CLIENT_TEST_PIN
VirtualTransceiver<TestRadioConfig, 'C', PRINT_CHANNEL_HOP, TX_CLIENT_TEST_PIN> ClientTransceiver(SchedulerBase);
#else
VirtualTransceiver<TestRadioConfig, 'C', PRINT_CHANNEL_HOP> ClientTransceiver(SchedulerBase);
#endif

HalfDuplex<DuplexPeriod, true, DuplexDeadZone> ClientDuplex;
ArduinoCycles ClientCyclesSource{};
LoLaAddressMatchLinkClient<> Client(SchedulerBase,
	&ClientTransceiver,
	&ClientCyclesSource,
	&ClientEntropySource,
	&ClientDuplex,
	&ClientChannelHop,
	SecretKey,
	AccessPassword,
	ClientAddress);

#if defined(LINK_TEST_DETUNE)
ClockDetunerTask Detuner(SchedulerBase);
#endif

#if defined(LINK_TEST_DISCOVERY)
DiscoveryTestService<'S', 0, 12345> ServerDiscovery(SchedulerBase, &Server);
DiscoveryTestService<'C', 0, 12345> ClientDiscovery(SchedulerBase, &Client);
#endif

#if defined(LINK_TEST_SURFACE)
ExampleSurface ReadSurface{};
SurfaceReader<1, 23456> ClientReader(SchedulerBase, &Client, &ReadSurface);
ExampleSurface WriteSurface{};
SurfaceWriter<1, 23456, DuplexPeriod / 1000> ServerWriter(SchedulerBase, &Server, &WriteSurface);
#if defined(USE_CONTROLLER)
ControllerSource<5> Controller(SchedulerBase, &Serial3);
ControllerExampleSurfaceDispatcher ControllerDispatcher(SchedulerBase, &Controller, &WriteSurface);
#endif
#endif

TestTask Tester(SchedulerBase, &Server, &Client);


#if defined(USE_LINK_DISPLAY)
VirtualLinkGraphicsEngine<ScreenDriverType, FrameBufferType>LinkDisplayEngine(&SchedulerBase, &Server, &Client, &ServerTransceiver, &ClientTransceiver);
#endif

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

#if defined(LINK_TEST_DISCOVERY)
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
#endif

#if defined(LINK_TEST_SURFACE)
	// Setup Test Sync Surface services.
	if (!ServerWriter.Setup())
	{
#ifdef DEBUG
		Serial.println(F("ServerWriter setup failed."));
#endif
		BootError();
	}
	if (!ClientReader.Setup())
	{
#ifdef DEBUG
		Serial.println(F("ClientReader setup failed."));
#endif
		BootError();
	}
#if defined(USE_CONTROLLER)
	Controller.Start();
	ControllerDispatcher.Start();
#endif
#endif

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
	if (Server.Start() && Client.Start())
	{
#ifdef DEBUG_LOLA_LINK
		Serial.print(millis());
		Serial.println(F("\tLoLa Links have started."));
#endif
	}
	else
	{
#ifdef DEBUG_LOLA_LINK
		Serial.println(F("Link Start Failed."));
#endif
		BootError();
	}

#if defined(USE_LINK_DISPLAY)
	LinkDisplayEngine.SetBufferTaskCallback(BufferTaskCallback);
	LinkDisplayEngine.SetInverted(false);
	LinkDisplayEngine.Start();
#endif
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}

#if defined(USE_LINK_DISPLAY)
void BufferTaskCallback(void* parameter)
{
	LinkDisplayEngine.BufferTaskCallback(parameter);
}
#endif