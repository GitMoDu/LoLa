/*
* Link Server example.
* Link objects and definitions in TransceiverDefinitions.
*/

#define DEBUG

#if defined(DEBUG)
#define DEBUG_LOLA
//#define DEBUG_LOLA_LINK
#define SERIAL_BAUD_RATE 115200
#endif

// Enable for experimental faster hash.
//#define LOLA_USE_POLY1305

// Enable to use channel hop. Disable for fixed channel.
//#define LINK_USE_CHANNEL_HOP

// Log the link status with the defined period (in ms).
#define LOLA_DEBUG_LINK_STATUS 5000

// Test Surface Service.
//#define LINK_TEST_SURFACE

// Enable to use ArduinoGraphicsEngine for displaying live link info.
// Depends on https://github.com/GitMoDu/ArduinoGraphicsEngine
//#define USE_LINK_DISPLAY
//#define GRAPHICS_ENGINE_MEASURE

// Selected Driver for test.
#define USE_NRF24_TRANSCEIVER


#define LINK_DUPLEX_SLOT false


#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN
#include <TScheduler.hpp>

#include <ILoLaTestingInclude.h>
#include <ILoLaInclude.h>


#if defined(LINK_TEST_SURFACE)
#include "../src/Testing/ExampleSurface.h"
#if defined(USE_NUNCHUK_CONTROLLER) || defined(USE_N64_CONTROLLER) || defined(USE_GAMECUBE_CONTROLLER)
#define USE_CONTROLLER 
#include "../src/Testing/ControllerSource.h"
#include "../src/Testing/ControllerExampleSurfaceDispatcher.h"
#endif
#endif

#if defined(USE_LINK_DISPLAY)
#include "../src/Display/LinkGraphicsEngine.h"
#endif

#if defined(LOLA_DEBUG_LINK_STATUS)
#include "Testing\LinkLogTask.h"
#endif

// Process scheduler.
TS::Scheduler SchedulerBase{};
//

// Transceiver.
#if defined(USE_SERIAL_TRANSCEIVER)
UartTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> Transceiver(SchedulerBase, SERIAL_TRANSCEIVER_INSTANCE);
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

#if defined(LINK_USE_CHANNEL_HOP) && !defined(LINK_FULL_DUPLEX_TRANSCEIVER)
TimedChannelHopper<ChannelHopPeriod> ChannelHop(SchedulerBase);
#endif

LoLaAddressMatchLinkServer<> LinkServer(SchedulerBase,
	&Transceiver,
	&CyclesSource,
	&EntropySource,
	&Duplex,
	&ChannelHop);

#if defined(LOLA_DEBUG_LINK_STATUS)
LinkLogTask<LOLA_DEBUG_LINK_STATUS> LogTask(&SchedulerBase, &LinkServer);
#endif

#if defined(LINK_TEST_SURFACE)
ExampleSurface ReadSurface{};
SurfaceReader<1, 23456> ServerReader(SchedulerBase, &Server, &ReadSurface);
#endif

#if defined(USE_LINK_DISPLAY)
//TwoWire& DisplayWire(Wire);
#if ARDUINO_MAPLE_MINI
Egfx::SpiType DisplaySpi(1);
#else
Egfx::SpiType& DisplaySpi(SPI);
#endif
ScreenDriverType ScreenDriver(DisplaySpi);
//ScreenDriverType ScreenDriver(DisplayWire);
LinkGraphicsEngine<FrameBufferType>LinkDisplayEngine(SchedulerBase, ScreenDriver, LinkServer, Transceiver);
#endif

void BootError()
{
#if defined(DEBUG)
	Serial.println("Critical Error");
#endif 
	delay(1000);
	while (1);;
}

void setup()
{
#if defined(DEBUG)
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
#endif

#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
	pinMode(SCHEDULER_TEST_PIN, OUTPUT);
#endif
#ifdef RX_TEST_PIN
	digitalWrite(RX_TEST_PIN, LOW);
	pinMode(RX_TEST_PIN, OUTPUT);
#endif
#ifdef TX_TEST_PIN
	digitalWrite(TX_TEST_PIN, LOW);
	pinMode(TX_TEST_PIN, OUTPUT);
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

#if defined(LINK_TEST_SURFACE)
	// Setup Test Sync Surface services.
	if (!ServerReader.Setup())
	{
#if defined(DEBUG)
		Serial.println(F("ServerReader setup failed."));
#endif
		BootError();
	}
#endif

	// Setup Link instance.
	if (!LinkServer.Setup(ServerAddress, AccessPassword, SecretKey))
	{
#if defined(DEBUG)
		Serial.println(F("LoLa Link Server Setup Failed."));
#endif
		BootError();
	}

#if defined(LOLA_DEBUG_LINK_STATUS)
	if (!LogTask.Setup())
	{
#if defined(DEBUG)
		Serial.println(F("LogTask Setup Failed."));
#endif
	}
#endif

	// Start Link instance.
	if (LinkServer.Start())
	{
		Serial.print(millis());
		Serial.println(F("\tLoLa Link Server Started."));
	}
	else
	{
#if defined(DEBUG)
		Serial.println(F("LoLa Link Server Start Failed."));
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

#if defined(USE_SERIAL_TRANSCEIVER) || defined(USE_NRF24_TRANSCEIVER) || defined(USE_SI446X_TRANSCEIVER) || defined(USE_SX12_TRANSCEIVER)
void LOLA_RTOS_INTERRUPT OnInterrupt()
{
	Transceiver.OnInterrupt();
}
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
void OnRxInterrupt(const uint8_t* mac, const uint8_t* buf, size_t count, void* arg)
{
	Transceiver.OnRxInterrupt(mac, buf, count);
}
#else
void OnRxInterrupt(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
	Transceiver.OnRxInterrupt(data, data_len);
}
void OnTxInterrupt(const uint8_t* mac_addr, esp_now_send_status_t status)
{
	Transceiver.OnTxInterrupt(status);
}
#endif
#endif