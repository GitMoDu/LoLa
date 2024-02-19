// ILoLaInclude.h

#ifndef _I_LOLA_INCLUDE_h
#define _I_LOLA_INCLUDE_h

#include <LoLaDefinitions.h>

#include "Link\ILoLaLink.h"
#include "Link\LoLaLinkDefinition.h"

//TODO: Refactor as individual driver includes
/// Available Packet Drivers
#include "LoLaTransceivers\VirtualTransceiver\VirtualTransceiver.h"
#include "LoLaTransceivers\UartTransceiver\UartTransceiver.h"
#include "LoLaTransceivers\nRF24Transceiver\nRF24Transceiver.h"
#include "LoLaTransceivers\Si446xTransceiver\Si446xTransceiver.h"
#include "LoLaTransceivers\Si446xTransceiver2\Si446xTransceiver2.h"
#include "LoLaTransceivers\Sx12Transceiver\Sx12Transceiver.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include "LoLaTransceivers\EspNowTransceiver\Esp8266NowTransceiver.h"
#elif defined(ARDUINO_ARCH_ESP32)
#include "LoLaTransceivers\EspNowTransceiver\Esp32NowTransceiver.h"
#endif
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_STM32F1)
#include "LoLaTransceivers\PimTransceiver\PimTransceiver.h"
#endif
///


/// Entropy Sources
#if defined(ARDUINO)
#include "EntropySources\ArduinoEntropy.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
#include "EntropySources\AvrEntropy.h"
#endif
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
#include "EntropySources\Stm32Entropy.h"
#endif
#if defined(ARDUINO_ARCH_ESP8266)
#include "EntropySources\Esp8266Entropy.h"
#endif
#if defined(ARDUINO_ARCH_ESP32)
//TODO: #include "EntropySources\Esp32Entropy.h"
#endif 
///


/// Clock Timer Sources
#if defined(ARDUINO)
#include "ClockSources\ArduinoCycles.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
//#include "TimerSources\AvrTimer1Source.h"
#endif
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
#include "ClockSources\Stm32SystemCycles.h"
#endif
#if defined(ARDUINO_ARCH_ESP8266)
#endif
#if defined(ARDUINO_ARCH_ESP32)
#endif
///


/// Available Link Modules
#include "LoLaLinks\LolaPkeLink\LoLaPkeLinkServer.h"
#include "LoLaLinks\LolaPkeLink\LoLaPkeLinkClient.h"

#include "LoLaLinks\LolaAddressMatchLink\LoLaAddressMatchLinkServer.h"
#include "LoLaLinks\LolaAddressMatchLink\LoLaAddressMatchLinkClient.h"

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