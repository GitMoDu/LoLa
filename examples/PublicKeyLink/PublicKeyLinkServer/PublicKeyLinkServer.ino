/*
* Publick Key Link Server
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

//#define LINK_USE_CHANNEL_HOP
//#define LINK_USE_TIMER_AND_RTC

// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 15
#define TEST_PIN_1 16
#define TEST_PIN_2 17

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define RX_TEST_PIN TEST_PIN_1
#define TX_TEST_PIN TEST_PIN_2
#endif

#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>

#include <ILoLaInclude.h>

// Transceiver Definitions.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 8
#define SERIAL_TRANSCEIVER_INSTANCE			Serial2
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF21_TRANSCEIVER)
#define NRF21_TRANSCEIVER_PIN_CE			3
#define NRF21_TRANSCEIVER_PIN_CS			7
#define NRF21_TRANSCEIVER_RX_INTERRUPT_PIN	1
#define NRF21_TRANSCEIVER_DATA_RATE			RF24_1MBPS
#endif
#else
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 2
#define SERIAL_TRANSCEIVER_INSTANCE			Serial
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF21_TRANSCEIVER)
#define NRF21_TRANSCEIVER_PIN_CE			9
#define NRF21_TRANSCEIVER_PIN_CS			10
#define NRF21_TRANSCEIVER_RX_INTERRUPT_PIN	2
#define NRF21_TRANSCEIVER_DATA_RATE			RF24_250KBPS
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

// Diceware created access control password.
static const uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Created offline using PublicPrivateKeyGenerator example project.
static const uint8_t ServerPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static const uint8_t ServerPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };
//

// Shared Link configuration.
static const uint16_t DuplexPeriod = 5000;
static const uint16_t DuplexDeadZone = 100;
static const uint32_t ChannelHopPeriod = DuplexPeriod * 4;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource EntropySource;
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266EntropySource EntropySource;
#else 
ArduinoEntropySource EntropySource;
#endif

#if !defined(LINK_USE_TIMER_AND_RTC)
ArduinoTaskTimerClockSource TimerClockSource(SchedulerBase);
#else
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32TimerSource TimerSource(1, 'A');
Stm32RtcClockSource ClockSource(SchedulerBase);
#elif defined(ARDUINO_ARCH_ESP8266)
#error No RTC/Timer sources found.
#else 
#error No RTC/Timer sources found.
#endif
#endif

#if defined(LINK_USE_CHANNEL_HOP) && !defined(USE_SERIAL_TRANSCEIVER)
TimedChannelHopper<ChannelHopPeriod> ChannelHop(SchedulerBase);
#else
NoHopNoChannel ChannelHop;
#endif

#if defined(USE_SERIAL_TRANSCEIVER)
FullDuplex Duplex{};
#else
HalfDuplex<DuplexPeriod, false, DuplexDeadZone> Duplex{};
#endif


LoLaPkeLinkServer<> Server(SchedulerBase,
	&TransceiverDriver,
	&EntropySource,
	&TimerClockSource,
	&TimerClockSource,
	&Duplex,
	&ChannelHop,
	ServerPublicKey,
	ServerPrivateKey,
	Password);


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

	// Setup Link instance.
	if (!Server.Setup())
	{
#ifdef DEBUG
		Serial.println(F("LoLa Link Server Setup Failed."));
#endif
		BootError();
	}

	// Start Link instance.
	if (Server.Start())
	{
		Serial.print(millis());
		Serial.println(F("\tLoLa Link Server Started."));
	}
	else
	{
#ifdef DEBUG
		Serial.println(F("LoLa Link Server Start Failed."));
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
