// ILoLaInclude.h

#ifndef _I_LOLA_INCLUDE_h
#define _I_LOLA_INCLUDE_h

#include <ILinkServiceInclude.h>

/// Entropy Sources
#if defined(ARDUINO)
#include "EntropySources/ArduinoEntropy.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
#include "EntropySources/AvrEntropy.h"
#elif defined(ARDUINO_ARCH_STM32F1)
#include "EntropySources/Stm32Entropy.h"
#elif defined(ARDUINO_ARCH_ESP8266)
#include "EntropySources/Esp8266Entropy.h"
#elif defined(ARDUINO_ARCH_ESP32)
#include "EntropySources/Esp32Entropy.h"
#endif 
///

/// Duplex Options
#include "Duplexes/Duplexes.h"
///

/// Channel Hopper Options
#include "ChannelHoppers/FixedHoppers.h"
#include "ChannelHoppers/TimedHoppers.h"
///

/// Clock Timer Sources
#if defined(ARDUINO)
#include "ClockSources/ArduinoCycles.h"
#endif
#if defined(ARDUINO_ARCH_AVR)
//#include "TimerSources/AvrTimer1Source.h"
#endif
#if defined(ARDUINO_ARCH_STM32F1)
#include "ClockSources/Stm32SystemCycles.h"
#endif
#if defined(ARDUINO_ARCH_ESP8266)
#endif
#if defined(ARDUINO_ARCH_ESP32)
#endif
///


/// Available Link Modules
#include "LoLaLinks\LolaAddressMatchLink/LoLaAddressMatchLinkServer.h"
#include "LoLaLinks\LolaAddressMatchLink/LoLaAddressMatchLinkClient.h"
///

/// Available Transceiver Drivers
#include "ILoLaTransceivers.h"
///

#endif