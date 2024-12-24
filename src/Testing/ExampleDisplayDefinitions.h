// ExampleDisplayDefinitions.h
#ifndef _EXAMPLE_DISPLAY_DEFINITIONS_h
#define _EXAMPLE_DISPLAY_DEFINITIONS_h

#include <ArduinoGraphicsDrivers.h>
#include <ArduinoGraphicsCore.h>

// Preset of SPI pin definitions for various platforms.
#if defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_STM32)
#define TFT_DC		PB0
#define TFT_RST     PB1
#define TFT_CS		PA4
#elif defined(ARDUINO_ARCH_STM32F1)
#define TFT_CS		7
#define TFT_DC		10
#define TFT_RST     11
#elif defined(ARDUINO_ARCH_AVR)
#define TFT_CS		10
#define TFT_DC		9
#define TFT_RST		8
#elif defined(ARDUINO_ARCH_ESP32)
#define TFT_CS		19
#define TFT_DC		20
#define TFT_RST		21
#elif defined(ARDUINO_SEEED_XIAO_RP2350)
#define TFT_CS		D3
#define TFT_DC		D7
#define TFT_RST		D6
#elif defined(ARDUINO_ARCH_RP2040)
#define TFT_CS		19
#define TFT_DC		20
#define TFT_RST		21
#elif defined(ARDUINO_ARCH_NRF52)
#define TFT_CS		7
#define TFT_DC		4
#define TFT_RST		5
#endif


#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_NRF52)
//using ScreenDriverType = ScreenDriverSSD1306_128x64x1_I2C_Rtos<>;
//using ScreenDriverType = ScreenDriverSSD1306_128x64x1_SPI_Rtos<TFT_CS, TFT_DC, TFT_RST>;
using ScreenDriverType = ScreenDriverSSD1306_128x64x1_SPI_Dma<TFT_CS, TFT_DC, TFT_RST, F_CPU / 8>;
using FrameBufferType = MonochromeFrameBuffer<ScreenDriverType::ScreenWidth, ScreenDriverType::ScreenHeight, 2, MonochromeColorConverter1<30>>;
#else
//using ScreenDriverType = ScreenDriverSSD1306_128x64x1_I2C;
//using ScreenDriverType = ScreenDriverSSD1306_128x64x1_SPI_Async<TFT_CS, TFT_DC, TFT_RST>;
//using ScreenDriverType = ScreenDriverSSD1306_128x64x1_SPI_Dma<TFT_CS, TFT_DC, TFT_RST>;
//using FrameBufferType = MonochromeFrameBuffer<ScreenDriverType::ScreenWidth, ScreenDriverType::ScreenHeight, 2, MonochromeColorConverter1<30>>;

//using ScreenDriverType = ScreenDriverSSD1331_96x64x8_SPI_Dma<TFT_CS, TFT_DC, TFT_RST, F_CPU / 2, 2>;
//using FrameBufferType = Color8FrameBuffer<ScreenDriverType::ScreenWidth, ScreenDriverType::ScreenHeight, 5>;

//using ScreenDriverType = ScreenDriverSSD1331_96x64x16_SPI_Dma<TFT_CS, TFT_DC, TFT_RST>;
//using FrameBufferType = Color16FrameBuffer<ScreenDriverType::ScreenWidth, ScreenDriverType::ScreenHeight, 5>;
#endif
#endif