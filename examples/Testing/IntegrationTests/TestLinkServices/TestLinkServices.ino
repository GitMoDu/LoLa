/* LoLa Link Virtual tester.
* Creates instances of both host and receiver,
* communicating through a virtual radio.
*
* Used to testing and developing the LoLa Link protocol.
*
*/

#define DEBUG
#define DEBUG_LOLA

#define SERIAL_BAUD_RATE 115200

#if !defined(LED_BUILTIN) && defined(ARDUINO_ARCH_ESP32)
#define LED_BUILTIN 33
#endif // !LED_BUILTIN




// Medium Simulation error chances, out 255, for every call.
//#define PRINT_PACKETS
//#define DROP_CHANCE 10
//#define CORRUPT_CHANCE 10
//#define ECO_CHANCE 5
//#define DOUBLE_SEND_CHANCE 5
#define HOST_DROP_LINK_TEST

#define LOLA_ENABLE_AUTO_CALIBRATE
#define LINK_USE_CHANNEL_HOP

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

// Packet Driver stuff.
// Fast GHz radio.
const uint32_t TxBase = 25;
const uint32_t TxByteNanos = 200;
const uint32_t RxBase = 15;
const uint32_t RxByteNanos = 200;
const uint8_t ChannelCount = 50;


// Shared Link stuff
static const uint16_t DuplexPeriod = 2000;
static const uint32_t ChannelHopPeriod = 20000;

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
Stm32EntropySource EntropySource;
#else
ArduinoEntropySource EntropySource;
#endif

// Diceware created access control password.
static uint8_t Password[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//

// Link host and its required instances.
VirtualHalfDuplexDriver<ChannelCount, TxBase, TxByteNanos, RxBase, RxByteNanos, false> HostDriver(SchedulerBase);

static uint8_t HostPublicKey[LoLaLinkDefinition::PUBLIC_KEY_SIZE] = { 0x88,0x64,0x9C,0xFE,0x38,0xFA,0xFE,0xB9,0x41,0xA8,0xD1,0xB7,0xAC,0xA0,0x23,0x82,0x97,0xFB,0x5B,0xD1,0xC4,0x75,0x94,0x68,0x41,0x6D,0xEE,0x57,0x6B,0x07,0xF5,0xC0,0x95,0x78,0x10,0xCC,0xEA,0x08,0x0D,0x8F };
static uint8_t HostPrivateKey[LoLaLinkDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x2E,0xBD,0x81,0x6E,0x56,0x59,0xDF,0x1D,0x77,0x83,0x0D,0x85,0xCE,0x59,0x61,0xE8,0x74,0x52,0xD7,0x98 };

#if defined(LINK_USE_CHANNEL_HOP)
TimedChannelHopper<ChannelHopPeriod> HostChannelHop(SchedulerBase);
#else
NoHopNoChannel HostChannelHop;
#endif
ArduinoMicrosClock<> HostTimerSource;
HalfDuplex<DuplexPeriod, true> HostDuplex;
LoLaLinkHost<> Host(SchedulerBase,
	&HostDriver,
	&EntropySource,
	&HostTimerSource,
	&HostDuplex,
	&HostChannelHop,
	HostPublicKey,
	HostPrivateKey,
	Password);

// Link Remote.
VirtualHalfDuplexDriver<ChannelCount, TxBase, TxByteNanos, RxBase, RxByteNanos, true> RemoteDriver(SchedulerBase);

static uint8_t RemotePublicKey[LoLaLinkDefinition::PUBLIC_KEY_SIZE] = { 0x21,0x9B,0xA6,0x2A,0xF0,0x14,0x8A,0x62,0xEB,0x2A,0xF0,0xD1,0xAC,0xB4,0x6E,0x15,0x1B,0x63,0xA7,0xEA,0x99,0xFD,0x1E,0xDD,0x74,0x2F,0xD4,0xB0,0xE1,0x04,0xC5,0x96,0x09,0x65,0x1F,0xAB,0x4F,0xDC,0x75,0x0C };
static uint8_t RemotePrivateKey[LoLaLinkDefinition::PRIVATE_KEY_SIZE] = { 0x00,0x9E,0x78,0xBA,0x67,0xEA,0x57,0xA9,0xBD,0x4E,0x1A,0x35,0xFB,0xD3,0xA7,0x19,0x29,0x55,0xB9,0xA1,0x3A };

#if defined(LINK_USE_CHANNEL_HOP)
TimedChannelHopper<ChannelHopPeriod> RemoteChannelHop(SchedulerBase);
#else
NoHopNoChannel RemoteChannelHop;
#endif
ArduinoMicrosClock<> RemoteTimerSource;
HalfDuplex<DuplexPeriod, false> RemoteDuplex;
LoLaLinkRemote<> Remote(SchedulerBase,
	&RemoteDriver,
	&EntropySource,
	&RemoteTimerSource,
	&RemoteDuplex,
	&RemoteChannelHop,
	RemotePublicKey,
	RemotePrivateKey,
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


	HostDriver.SetPartner(&RemoteDriver);
	RemoteDriver.SetPartner(&HostDriver);

	if (!Host.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Host Link Setup Failed."));
#endif
		BootError();
	}

	if (!Remote.Setup())
	{
#ifdef DEBUG
		Serial.println(F("Remote Link Setup Failed."));
#endif
		BootError();
	}

	if (Host.Start() &&
		Remote.Start())
	{
		Serial.print(micros());
		Serial.println(F(": LoLa Links started."));
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
	SchedulerBase.execute();
}
