// ExampleTransceiverDefinitions.h
#ifndef _EXAMPLE_TRANSCEIVER_DEFINITIONS_h
#define _EXAMPLE_TRANSCEIVER_DEFINITIONS_h

#include <SPI.h>

//#define USE_SERIAL_TRANSCEIVER
//#define USE_NRF24_TRANSCEIVER
//#define USE_ESPNOW_TRANSCEIVER
//#define USE_SI446X_TRANSCEIVER
//#define USE_SX12_TRANSCEIVER

#define LINK_USE_CHANNEL_HOP


#if defined(USE_SERIAL_TRANSCEIVER)
#define LINK_FULL_DUPLEX_TRANSCEIVER
#endif

// Shared Link configuration.
static constexpr uint16_t DuplexPeriod = 10000;
static constexpr uint16_t DuplexDeadZone = 150;
static constexpr uint32_t ChannelHopPeriod = DuplexPeriod;

// Diceware created access control password.
static constexpr uint8_t AccessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x10, 0x01, 0x20, 0x02, 0x30, 0x03, 0x40, 0x04 };
static constexpr uint8_t SecretKey[LoLaLinkDefinition::SECRET_KEY_SIZE] = { 0x50, 0x05, 0x60, 0x06, 0x70, 0x07, 0x80, 0x08 };
//

// Diceware created values for address and secret key.
static constexpr uint8_t ServerAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
static constexpr uint8_t ClientAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };


#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 8
#define SERIAL_TRANSCEIVER_INSTANCE			Serial2
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_SPI_CHANNEL		1
#define NRF24_TRANSCEIVER_PIN_CE			13
#define NRF24_TRANSCEIVER_PIN_CS			12
#define NRF24_TRANSCEIVER_INTERRUPT_PIN		14
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			31
#define SI446X_TRANSCEIVER_PIN_SDN			22
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	21
#if defined(ARDUINO_ARCH_STM32F1)
SPIClass TransceiverSpi(2);
#else
#define TransceiverSpi SPI
#endif
#define TRANSCEIVER_SPI						TransceiverSpi
#elif defined(USE_SX12_TRANSCEIVER)
#define SX12_TRANSCEIVER_SPI_CHANNEL		2
#define SX12_TRANSCEIVER_PIN_CS				31
#define SX12_TRANSCEIVER_PIN_SDN			22
#define SX12_TRANSCEIVER_RX_INTERRUPT_PIN	21
#endif

#else
#if defined(USE_SERIAL_TRANSCEIVER)
#define SERIAL_TRANSCEIVER_RX_INTERRUPT_PIN 2
#define SERIAL_TRANSCEIVER_INSTANCE			Serial
#elif defined(USE_NRF24_TRANSCEIVER)
#define NRF24_TRANSCEIVER_SPI_CHANNEL		0
#define NRF24_TRANSCEIVER_PIN_CS			10
#define NRF24_TRANSCEIVER_PIN_CE			9
#define NRF24_TRANSCEIVER_INTERRUPT_PIN		2
#elif defined(USE_SI446X_TRANSCEIVER)
#define SI446X_TRANSCEIVER_PIN_CS			10
#define SI446X_TRANSCEIVER_PIN_SDN			9
#define SI446X_TRANSCEIVER_RX_INTERRUPT_PIN	2
#elif defined(USE_SX12_TRANSCEIVER)
#define SX12_TRANSCEIVER_PIN_CS				10
#define SX12_TRANSCEIVER_PIN_RST			9
#define SX12_TRANSCEIVER_PIN_BUSY			8
#define SX12_TRANSCEIVER_INTERRUPT_PIN		2
#endif
#endif

#if defined(USE_ESPNOW_TRANSCEIVER)
#if defined(ARDUINO_ARCH_ESP8266)
#elif defined(ARDUINO_ARCH_ESP32)
#else
#error "USE_ESPNOW_TRANSCEIVER Is only available for the ESP32 and ESP8266 Arduino core platforms."
#endif
#endif


// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1)
Stm32Entropy EntropySource{};
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266Entropy EntropySource{};
#elif defined(ARDUINO_ARCH_ESP32)
Esp32Entropy EntropySource{};
#else 
ArduinoLowEntropy EntropySource{};
#endif

ArduinoCycles CyclesSource{};


// Use timed hopper when LINK_USE_CHANNEL_HOP but not on Full-Duplex.
#if !defined(LINK_USE_CHANNEL_HOP) || defined(LINK_FULL_DUPLEX_TRANSCEIVER)
NoHopNoChannel ChannelHop{};
#endif

// Duplex Slot must be set if using half-duplex.
#if defined(LINK_FULL_DUPLEX_TRANSCEIVER)
FullDuplex<DuplexPeriod> Duplex{};
#else
#if defined(LINK_DUPLEX_SLOT)
HalfDuplex<DuplexPeriod, LINK_DUPLEX_SLOT, DuplexDeadZone> Duplex{};
#else
#error LINK_DUPLEX_SLOT Must be #defined to true or false.
#endif
#endif

#endif