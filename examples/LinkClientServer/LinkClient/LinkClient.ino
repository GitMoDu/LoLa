/*
* Link Client example.
* Enable LINK_USE_PKE_LINK for Public-Key Exchange Link,
* disable for Address Match Link.
* Link objects and definitions in TransceiverDefinitions.
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
#define _TASK_SLEEP_ON_IDLE_RUN

// Enable for Public-Key Exchange Link.
// Disable for Address Match Link.
#define LINK_USE_PKE_LINK

//#define LINK_USE_CHANNEL_HOP
//#define LINK_USE_TIMER_AND_RTC

// Selected Driver for test.
#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER2

#define LINK_DUPLEX_SLOT true

#include <TaskScheduler.h>
#include <ILoLaInclude.h>

#include "Testing\ExampleTransceiverDefinitions.h"
#include "Testing\ExampleLogicAnalyserDefinitions.h"

// Process scheduler.
Scheduler SchedulerBase;
//

#if defined(LINK_USE_CHANNEL_HOP) && !defined(LINK_FULL_DUPLEX_TRANSCEIVER)
TimedChannelHopper<ChannelHopPeriod> ChannelHop(SchedulerBase);
#endif

// Transceiver Driver.
#if defined(USE_SERIAL_TRANSCEIVER)
UartTransceiver<HardwareSerial, SERIAL_TRANSCEIVER_BAUDRATE, SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SERIAL_TRANSCEIVER_INSTANCE);
#elif defined(USE_NRF24_TRANSCEIVER)
SPIClass SpiTransceiver(NRF24_TRANSCEIVER_SPI_CHANNEL);
nRF24Transceiver<NRF24_TRANSCEIVER_PIN_CE, NRF24_TRANSCEIVER_PIN_CS, NRF24_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SpiTransceiver);
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
EspNowTransceiver TransceiverDriver(SchedulerBase);
#else
EspNowTransceiver TransceiverDriver(SchedulerBase);
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
Si446xTransceiver<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase);
#elif defined(USE_SI446X_TRANSCEIVER2)
SPIClass SpiTransceiver(SI446X_TRANSCEIVER_SPI_CHANNEL);
Si446xTransceiver2<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase, &SpiTransceiver);
#else
#error No Transceiver.
#endif
//

// Diceware created access control password.
static const uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Created offline using PublicPrivateKeyGenerator example project.
static const uint8_t ClientPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x21,0x9B,0xA6,0x2A,0xF0,0x14,0x8A,0x62,0xEB,0x2A,0xF0,0xD1,0xAC,0xB4,0x6E,0x15,0x1B,0x63,0xA7,0xEA,0x99,0xFD,0x1E,0xDD,0x74,0x2F,0xD4,0xB0,0xE1,0x04,0xC5,0x96,0x09,0x65,0x1F,0xAB,0x4F,0xDC,0x75,0x0C };
static const uint8_t ClientPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x9E,0x78,0xBA,0x67,0xEA,0x57,0xA9,0xBD,0x4E,0x1A,0x35,0xFB,0xD3,0xA7,0x19,0x29,0x55,0xB9,0xA1,0x3A };
//

#if defined(LINK_USE_PKE_LINK)
LoLaPkeLinkClient<> Client(SchedulerBase,
	&TransceiverDriver,
	&CyclesSource,
	&EntropySource,
	&Duplex,
	&ChannelHop,
	Password,
	ClientPublicKey,
	ClientPrivateKey);
#else
LoLaAddressMatchLinkClient<> Client(SchedulerBase,
	&TransceiverDriver,
	&CyclesSource,
	&EntropySource,
	&Duplex,
	&ChannelHop,
	Password,
	ClientPublicKey);
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

	pinMode(LED_BUILTIN, OUTPUT);

#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
	pinMode(SCHEDULER_TEST_PIN, OUTPUT);
#endif

#if defined(USE_SERIAL_TRANSCEIVER)
	TransceiverDriver.SetupInterrupt(OnSeriaInterrupt);
#elif defined(USE_NRF24_TRANSCEIVER)
	SpiTransceiver.begin();
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

	// Setup Link instance.
	if (!Client.Setup())
	{
#ifdef DEBUG
		Serial.println(F("LoLa Link Client Setup Failed."));
#endif
		BootError();
	}

	// Start Link instance.
	if (Client.Start())
	{
#if !defined(LOLA_DEBUG_LINK_CLOCK)
		Serial.print(millis());
		Serial.println(F("\tLoLa Link Client Started."));
#endif
	}
	else
	{
#ifdef DEBUG
		Serial.println(F("LoLa Link Client Start Failed."));
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
// Actual interrupt.
void OnRadioInterrupt()
{
	TransceiverDriver.OnRadioInterrupt();
}

// Global calls fired in event loop.
void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	TransceiverDriver.OnPostRxInterrupt(length, rssi);
}
void SI446X_CB_RXBEGIN(int16_t rssi)
{
	TransceiverDriver.OnPreRxInterrupt();
}
void SI446X_CB_SENT(void)
{
	TransceiverDriver.OnTxInterrupt();
}
#elif defined(USE_SI446X_TRANSCEIVER2)
void OnRadioInterrupt()
{
	TransceiverDriver.OnRadioInterrupt();
}
#endif