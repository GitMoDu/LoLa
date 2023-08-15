// ILoLaInclude.h

#ifndef _I_LOLA_INCLUDE_h
#define _I_LOLA_INCLUDE_h

#if !defined(ARDUINO)
#error Arduino Required for LoLa Library
#endif

#include "Link\ILoLaLink.h"
#include "Link\LoLaLinkDefinition.h"

//TODO: Refactor as individual driver includes
/// Available Packet Drivers
#include "LoLaTransceivers\VirtualTransceiver\VirtualTransceiver.h"
#include "LoLaTransceivers\UartTransceiver\UartTransceiver.h"
#include "LoLaTransceivers\nRF24Transceiver\nRF24Transceiver.h"
#include "LoLaTransceivers\Si446xTransceiver\Si446xTransceiver.h"
#include "LoLaTransceivers\EspNowTransceiver\EspNowTransceiver.h"
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_STM32F1)
#include "LoLaTransceivers\PimTransceiver\PimTransceiver.h"
#else 
#warning No Implementation available for PIM transceiver.
#endif
///


/// Entropy Sources
#if defined(ARDUINO)
#include "EntropySources\ArduinoEntropySource.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
#include "EntropySources\AvrEntropySource.h"
#endif
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
#include "EntropySources\Stm32EntropySource.h"
#endif
#if defined(ARDUINO_ARCH_ESP8266)
#include "EntropySources\Esp8266EntropySource.h"
#endif
#if defined(ARDUINO_ARCH_ESP32)
//#include "EntropySources\Esp32EntropySource.h"
#endif 
///


/// Clock Timer Sources
#if defined(ARDUINO)
#include "ClockSources\ArduinoTaskTimerClockSource.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
#include "TimerSources\AvrTimer1Source.h"
#endif
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
#include "TimerSources\Stm32TimerSource.h"
#include "ClockSources\Stm32RtcClockSource.h"
#endif
#if defined(ARDUINO_ARCH_ESP8266)
//#include "TimerSources\Esp8266TimerSource.h"
#endif
#if defined(ARDUINO_ARCH_ESP32)
//#include "TimerSources\Esp32TimerSource.h"
#endif
///


/// Available Link Modules
#include "LoLaLinks\LolaPkeLink\LoLaPkeLinkServer.h"
#include "LoLaLinks\LolaPkeLink\LoLaPkeLinkClient.h"
//#include "LoLaLinkLight\LoLaLinkLight.h"

/// Available Link Based Services
#if defined(LOLA_DEBUG)
//#include "Services\Debug\DiscoveryTestService.h"
#endif
#include "LoLaServices\SyncSurface\SyncSurfaceReader.h"
#include "LoLaServices\SyncSurface\SyncSurfaceWriter.h"
//#include "Services\BinaryDelivery\BinaryDeliveryService.h"
//#include "Services\Stream\ForwardCorrectedLossyStream.h"
///

/// Duplex Options
#include "Duplexes\Duplexes.h"
///


/// Channel Hopper Options
#include "ChannelHoppers\FixedHoppers.h"
#include "ChannelHoppers\TimedHoppers.h"
///

#endif