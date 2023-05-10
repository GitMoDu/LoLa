// ILoLaInclude.h

#ifndef _I_LOLA_INCLUDE_h
#define _I_LOLA_INCLUDE_h


#include "Link\ILoLaLink.h"
#include "Link\LoLaLinkDefinition.h"

//TODO: Refactor as individual driver includes?
#pragma	region Available Packet Drivers
#include "LoLaPacketDrivers\VirtualPacketDriver\VirtualHalfDuplexDriver.h"
//#include "LoLaPacketDrivers\SerialLoLaPacketDriver\SerialLoLaPacketDriver.h"
//#include "LoLaPacketDrivers\PIMLoLaPacketDriver\PIMLoLaPacketDriver.h"
//#include "LoLaPacketDrivers\Si446xLolaPacketDriver\Si446xLolaPacketDriver.h"
#pragma endregion


#pragma	region Entropy Sources
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
#pragma endregion


#pragma	region Clock Timer Sources
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
#pragma endregion


#pragma	region Available Link modules
#include "LoLaLinks\LinkHostRemote\LoLaLinkHost.h"
#include "LoLaLinks\LinkHostRemote\LoLaLinkRemote.h"
//#include "LoLaLinkLight\LoLaLinkLight.h"
#pragma endregion

#pragma	region Available Link Based Services
#if defined(LOLA_DEBUG)
//#include "Services\Debug\DiscoveryTestService.h"
#endif
#include "LoLaServices\SyncSurface\SyncSurfaceReader.h"
#include "LoLaServices\SyncSurface\SyncSurfaceWriter.h"
//#include "Services\BinaryDelivery\BinaryDeliveryService.h"
//#include "Services\Stream\ForwardCorrectedLossyStream.h"
#pragma endregion

#pragma	region Duplex Options
#include "Duplexes\Duplexes.h"
#pragma endregion


#pragma	region Channel Hopper Options
#include "ChannelHoppers\FixedHoppers.h"
#include "ChannelHoppers\TimedHoppers.h"
#pragma endregion


#endif