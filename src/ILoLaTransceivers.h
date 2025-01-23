// ILoLaTransceivers.h

#ifndef _I_LOLA_TRANSCEIVERS_INCLUDE_h
#define _I_LOLA_TRANSCEIVERS_INCLUDE_h

/// Available Transceiver Drivers
#include "LoLaTransceivers/VirtualTransceiver/VirtualTransceiver.h"
#include "LoLaTransceivers/UartTransceiver/UartTransceiver.h"
#include "LoLaTransceivers/nRF24Transceiver/nRF24Transceiver.h"
#include "LoLaTransceivers/Si446xTransceiver/Si446xTransceiver.h"
//#include "LoLaTransceivers\Sx12Transceiver\Sx12Transceiver.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include "LoLaTransceivers/EspNowTransceiver/Esp8266NowTransceiver.h"
#elif defined(ARDUINO_ARCH_ESP32)
#include "LoLaTransceivers/EspNowTransceiver/Esp32NowTransceiver.h"
#endif
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_STM32F1)
#include "LoLaTransceivers/PimTransceiver/PimTransceiver.h"
#endif
///

#endif