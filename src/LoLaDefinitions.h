// LoLaDefinitions.h

#ifndef _LOLA_DEFINITIONS_h
#define _LOLA_DEFINITIONS_h

#if defined(DEBUG_LOLA)
// Debug output enabled.
#endif

#if defined(DEBUG_LOLA_LINK)
// Link debug output enabled.
#endif

#if defined(LOLA_USE_POLY1305)
// Use Poly1305 hash, instead of Xoodyak.
#endif

#if !defined(ARDUINO)
#error Arduino HAL is required for LoLa Library.
#endif


#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_STM32)
#define LOLA_RTOS_PAUSE()	((void)0)
#define LOLA_RTOS_RESUME()	((void)0)
#define LOLA_RTOS_INTERRUPT
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
#include <FreeRTOS.h>
#include <task.h>
#define LOLA_RTOS_PLATFORM
#define _TASK_THREAD_SAFE	// Enable additional checking for thread safety
#define LOLA_RTOS_PAUSE()	vTaskSuspendAll()
#define LOLA_RTOS_RESUME()	xTaskResumeAll()
#define LOLA_RTOS_INTERRUPT IRAM_ATTR
#else
#error Platform not suported.
#endif

#if defined(DEBUG_LOLA_LINK) && defined(LOLA_RTOS_PLATFORM)
#error  Link debug cannot be enabled on a RTOS platform.
#endif

#endif