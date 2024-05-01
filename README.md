
# Low Latency Link (LoLa) 


## Software defined Link stack for Arduino HAL.

![](https://raw.githubusercontent.com/GitMoDu/LoLa/master/media/First_tests.jpg)


# Goal for this project

Create a seamless bi-directional communication, real-time system, for Remote Control and Robotics applications (Example: RC car and remote).

# Supported Platforms: 

- STM32F1
- ESP32
- RP2040
- MegaAVR

# Technical highlights
- Synchonized clock between partners (+- 20us).
- Fast encryption and MAC (~200us on ARM M3).
- Collision avoidance with synchronized Duplex (period is configurable).
- Optional full spectrum frequency hopping (period is configurable).
- No low-level Tx retry mechanism.

# Cryptography
- Perfect forward secrecy (each session uses unique encoding).
- 32 bit MAC with integrity and authenticity validation.
- MAC-then-Encrypt for fast rejection without decryption.
- 1-second rolling one-time-password combined with 16 bit counter, supports up 65535 packets/s before Nonce re-use.
- No CRC needed on Transceiver.


# LinkService
TemplateLinkService:
- Exposes service to link-enabled actions (send packet, register receive port, etc..).
- Provides asynchronous send packet. Tx occurs only when in slot and the transceiver is ready for Tx, with last moment OnPreSend call (for time sensitive payloads).
- Priority levels for send (1-255).

AbstractDiscoveryService
  - Extends TemplateLinkService exposes service to actions after partner service is present, validated and synchronized

## Implemented services
SyncSurface Service
- Synchronises an array of N blocks (32bits wide) in a differential fashion, i.e., only send over radio the changing blocks, not the whole data array. There's a 2-way protocol to ensure data integrity without compromising data update latency.
 

# Supported Transceivers
- nRF24L01+ (depends on https://github.com/nRF24/RF24)
- Si4463 (native driver and configurator)
- UART (native driver)
- VirtualTransceiver (for development and testing)
- Esp32 ESPNow [WORK_IN_PROGRESS]
- Sx12 LoRa [WORK_IN_PROGRESS]


Drunk rebuild. Don't ask, I wasn't happy with a lot of stuff, so I changed a lot of stuff. Now nothing works, but it's fine, it will.
  - Custom radio driver: no more hogging the interrupt.
  - Fully async packet processing: no more worrying that taking too long to eat a packet will make us lose the next one.
  - Revamp crypto with modern practices: KDF and standard hashing.
  - Simplify radio driver model: simplify radio driver implementation.
  - Optimize blocking operations: reduce the key generations to once per boot or per lifetime.
  - Reliable sub-second time with RTC: a 16 bit timer provides enough resolution (< 50 us).
  - Optimize packet map and definitions: less setupy, more constructy.
  - Improve replay attack protection: eventually I will decide to add a counter/padding to the packets. Not today.




