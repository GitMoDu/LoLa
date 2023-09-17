/*
* Publick Key Link Server
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

#define LINK_USE_PKE_LINK

// Selected Driver for test.
#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER

#define LINK_USE_CHANNEL_HOP
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
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP32)
#define _TASK_THREAD_SAFE
#else
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
#if defined(ARDUINO_ARCH_ESP8266)
EspNowTransceiver TransceiverDriver(SchedulerBase);
#else
EspNowTransceiver<ESPNOW_TRANSCEIVER_DATA_RATE> TransceiverDriver(SchedulerBase);
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
Si446xTransceiver<SI446X_TRANSCEIVER_PIN_CS, SI446X_TRANSCEIVER_PIN_SDN, SI446X_TRANSCEIVER_RX_INTERRUPT_PIN> TransceiverDriver(SchedulerBase);
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
static const uint16_t DuplexPeriod = 12000;
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
ArduinoCycles CyclesSource{};
#else
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32TimerCycles CyclesSource{};
#elif defined(ARDUINO_ARCH_ESP8266)
#error No CyclesSource found.
#else 
#error No CyclesSource found.
#endif
#endif

#if defined(LINK_USE_CHANNEL_HOP) && !defined(USE_SERIAL_TRANSCEIVER)
TimedChannelHopper<ChannelHopPeriod> ChannelHop(SchedulerBase);
#else
NoHopNoChannel ChannelHop{};
#endif

#if defined(USE_SERIAL_TRANSCEIVER)
FullDuplex Duplex{};
#else
HalfDuplex<DuplexPeriod, false, DuplexDeadZone> Duplex{};
#endif

#if defined(LINK_USE_PKE_LINK)
LoLaPkeLinkServer<> Server(SchedulerBase,
	&TransceiverDriver,
	&CyclesSource,
	&EntropySource,
	&Duplex,
	&ChannelHop,
	Password,
	ServerPublicKey,
	ServerPrivateKey);
#else
LoLaAddressMatchLinkServer<> Server(SchedulerBase,
	&TransceiverDriver,
	&CyclesSource,
	&EntropySource,
	&Duplex,
	&ChannelHop,
	Password,
	ServerPublicKey);
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
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
	pinMode(SCHEDULER_TEST_PIN, OUTPUT);
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