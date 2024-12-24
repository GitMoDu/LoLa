// ExampleLogicAnalyserDefinitions.h
#ifndef _EXAMPLE_LOGIC_ANALYSER_DEFINITIONS_h
#define _EXAMPLE_LOGIC_ANALYSER_DEFINITIONS_h

// Test pins for logic analyser.
#if defined(ARDUINO_ARCH_STM32F1)
#define TEST_PIN_0 17
#define TEST_PIN_1 18
#define TEST_PIN_2 19
#define TEST_PIN_3 20

#define SCHEDULER_TEST_PIN TEST_PIN_0
#define RX_TEST_PIN TEST_PIN_1
#define TX_TEST_PIN TEST_PIN_2
#define HOP_TEST_PIN TEST_PIN_3
#endif

#endif