/* LoLa Link Host tester.
* Creates instances for host.
* Used to testing and developing the LoLa Link protocol.
*
*/

#define DEBUG

#define SERIAL_BAUD_RATE 115200



#define DEBUG_LOLA



#define _TASK_SCHEDULE_NC // Disable task catch-up.
#define _TASK_OO_CALLBACKS

#ifndef ARDUINO_ARCH_ESP32
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass.
#endif

#include <TaskScheduler.h>

#include <ILoLaInclude.h>


// Process scheduler.
Scheduler SchedulerBase;
//


// Diceware created access control password.
static uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Created offline using PublicPrivateKeyGenerator example project.
static uint8_t HostPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static uint8_t HostPrivateKey[LoLaCryptoDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };
//

// Shared Link configuration.
static const uint16_t DuplexPeriod = 10000;
static const uint32_t ChannelHopPeriod = 20000;

// Use best available sources.
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource EntropySource;
#elif defined(ARDUINO_ARCH_ESP8266)
Esp8266EntropySource EntropySource;
#else 
ArduinoEntropySource EntropySource;
#endif


ArduinoTaskTimerClockSource<> SharedTimerClockSource(SchedulerBase);
IClockSource* HostClock = &SharedTimerClockSource;
ITimerSource* HostTimer = &SharedTimerClockSource;


// Link host and its required instances.
NoHopNoChannel HostChannelHop;
HalfDuplex<DuplexPeriod, false> HostDuplex;
LoLaLinkHost<> Host(SchedulerBase,
	nullptr,
	&EntropySource,
	HostClock,
	HostTimer,
	&HostDuplex,
	&HostChannelHop,
	HostPublicKey,
	HostPrivateKey,
	Password);


void BootError()
{
#ifdef DEBUG
	Serial.println("Critical Error");
#endif  
	delay(1000);
	while (1);;
}

void setup()
{
#ifdef DEBUG
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
#endif

	// Setup Link instance.
	if (!Host.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Host Link Setup Failed."));
#endif
		BootError();
	}


	// Start Link intance.
	if (Host.Start())
	{
		Serial.print(millis());
		Serial.println(F("\tLoLa Links have started."));
	}
	else
	{
#ifdef DEBUG
		Serial.println(F("Link Start Failed."));
#endif
		BootError();
	}
}

void loop()
{
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, HIGH);
#endif
#ifdef SCHEDULER_TEST_PIN
	digitalWrite(SCHEDULER_TEST_PIN, LOW);
#endif
	SchedulerBase.execute();
}
