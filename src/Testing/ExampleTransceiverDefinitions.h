// ExampleTransceiverDefinitions.h
#ifndef _EXAMPLE_TRANSCEIVER_DEFINITIONS_h
#define _EXAMPLE_TRANSCEIVER_DEFINITIONS_h

//#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER

//#define LINK_USE_CHANNEL_HOP
//#define LINK_USE_TIMER_AND_RTC

// Shared Link configuration.
static const uint16_t DuplexPeriod = 10000;
static const uint16_t DuplexDeadZone = DuplexPeriod / 20;
static const uint32_t ChannelHopPeriod = DuplexPeriod;

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
#if defined(USE_SERIAL_TRANSCEIVER)
#define LINK_FULL_DUPLEX_TRANSCEIVER		// Only Full-Duplex transceiver available.
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 8
#define SERIAL_TRANSCEIVER_INSTANCE			Serial2
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_SPI_CHANNEL		1
#define NRF24_TRANSCEIVER_PIN_CE			13
#define NRF24_TRANSCEIVER_PIN_CS			12
#define NRF24_TRANSCEIVER_RX_INTERRUPT_PIN	14
#elif defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
#elif defined(ARDUINO_ARCH_ESP32)
#else
#error "USE_ESPNOW_TRANSCEIVER Is only available for the ESP32 and ESP8266 Arduino core platforms."
#endif
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_SPI_CHANNEL		1
#define SI446X_TRANSCEIVER_PIN_CS			7
#define SI446X_TRANSCEIVER_PIN_SDN			9
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	8
#elif defined(USE_SI446X_TRANSCEIVER2)
#define SI446X_TRANSCEIVER_SPI_CHANNEL		2
#define SI446X_TRANSCEIVER_PIN_CS			31
#define SI446X_TRANSCEIVER_PIN_SDN			25
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	26
#endif
#else
#if defined(USE_SERIAL_TRANSCEIVER)
#define LINK_FULL_DUPLEX_TRANSCEIVER		// Only Full-Duplex transceiver available.
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 2
#define SERIAL_TRANSCEIVER_INSTANCE			Serial
#define SERIAL_TRANSCEIVER_BAUDRATE			115200
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_PIN_CS			10
#define NRF24_TRANSCEIVER_PIN_CE			9
#define NRF24_TRANSCEIVER_RX_INTERRUPT_PIN	2
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			10
#define SI446X_TRANSCEIVER_PIN_SDN			9
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	2
#elif defined(USE_SI446X_TRANSCEIVER2)
#define SI446X_TRANSCEIVER_PIN_CS			10
#define SI446X_TRANSCEIVER_PIN_SDN			9
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	2
#endif
#endif


// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32Entropy EntropySource{};
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266Entropy EntropySource{};
#else 
ArduinoEntropy EntropySource{};
#endif

// Use best source only if specified by LINK_USE_TIMER_AND_RTC.
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

// Use timed hopper when LINK_USE_CHANNEL_HOP but not on Full-Duplex.
#if !defined(LINK_USE_CHANNEL_HOP) || defined(LINK_FULL_DUPLEX_TRANSCEIVER)
NoHopNoChannel ChannelHop{};
#endif

// Duplex Slot must be set if using half-duplex.
#if defined(LINK_FULL_DUPLEX_TRANSCEIVER)
FullDuplex Duplex{};
#else
#if defined(LINK_DUPLEX_SLOT)
HalfDuplex<DuplexPeriod, LINK_DUPLEX_SLOT, DuplexDeadZone> Duplex{};
#else
#error LINK_DUPLEX_SLOT Must be #defined to true or false.
#endif
#endif
//

#endif