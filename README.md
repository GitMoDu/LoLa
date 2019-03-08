# LoLa 

  

# Low Latency Radio System 

![](https://raw.githubusercontent.com/GitMoDu/LoLa/master/media/First_tests.jpg)



For Arduino compatible boards. 
  

# This project would not be possible (and actually won't compile) without these contributions

 
Radio Driver for Si446x: https://github.com/zkemble/Si446x 

Ring Buffer: https://github.com/wizard97/Embedded_RingBuf_CPP 

Generic Callbacks: https://github.com/tomstewart89/Callback 

OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler

Fast CRC calculator: https://github.com/FrankBoesing/FastCRC.git
Tiny CRC calculator fork: https://github.com/GitMoDu/TinyCRC

Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git

Micro Elyptic Curve Cryptography: https://github.com/kmackay/micro-ecc

Crypto Library for modern embedded Cyphers: https://github.com/rweather/arduinolibs





# Goal for this project

Make seamlessly bi-directional communicating real-time system, for Remote Control and Robotics applications (Example: RC car and remote), with the least possible latency while still providing room for lots of "channels".

Gist of the Packet Driver: 
When receiving, it tries to throw the incoming packet to one of the registered LoLa Services. There is no callback for when no registered service wants the packet. 

The packets definitions and payload are defined in a very minimalistic way with only some configurability.
  


# Link Features

Driver for Si4463 [WORKING]: The radio IC this project is based around, but not limited to.

Simple Acknowledged Packets with Id [WORKING]: carrying only the original packet's header and optional id. Currently only used for establishing a link, as the Ack packets ignore the collision-avoidance setup. This way we can accuratelly measure total system latency before the link is ready.

Synchronized clock [WORKING]: when establishing a link, the Remote's clock is synced to the Host's clock. The host clock is randomized for each new link session. The clock is tuned during link time. Possible improvements: get host/remote clock delta.

Packet collision avoidance [WORKING]: with the Synchronized clock, we split a fixed period in half where the Host can only transmit during the first half and the Remote during the second half (half-duplex). Default duplex period is 10 milliseconds. Latency is taken into account for this feature (optional).

Unbuffered Output [WORKING]: Each LoLa service can handle a packet send being delayed or even failed, so we don't need to buffer outputs. IPacketSendService extends the base ILoLaService and provides overloads for extension.

Link Handshake Handling [WORKING] – Broadcast Id and find a partner. Clock is synced, Crypto tokens and basic link info is exchanged.

Link management [WORKING]: Link service establishes a link and fires events when the link is gained or lost. Keeps sending pings (with replies) to make sure the partner is still there, avoiding stealing bandwidth from user services.

Latency Compensation for Link[WORKING]: Using the measured latency, we use the estimated transmission time to optimize time dependent values. This feature is awesome, it decreased packet collision from 1.5% to 0.6% .

Transmit Power Balancer[WORKING]: with the link up, the end-points are continuosly updated on the RSSI of the partner, and adjusts the output power conservatively.

Simulated Packet Loss for Testing[WORKING]: This feature allows us to test the system in simulated bad conditions.

Link diagnostics [WORKING]: Measures link properties: Latency, RSSI, Remote RSSI. Not included in Link by default, only used for diagnose and debug the radio IC.


# Implemented services

SyncSurface Service [WORKING]: The star feature and whole reason I started this project. Synchronises an array of N blocks (32bits wide) in a differential fashion, i.e., only send over radio the changing blocks, not the whole data array. There's a 2-way protocol to ensure data integrity without compromising data update latency.


# Why not Radiohead or similar radio libraries? 

I'd like to build a real time system, biasing the implementation for low latency and code readability. Besides, it's fun. 

 


# Notable issues

It's quite a memory hog (around 1.5 kB for the example projects with only 2 sync surfaces), and ROM but there's a lot of functionality. 

 


# Future : 
Async Radio Driver [IN_PROGRESS] – Perform the long packet processing in the main loop, instead of hogging the interrupt.

Cryptography[IN_PROGRESS] – Linking doesn't start until an Elyptic Curve Diffie-Hellman key exchange is done. Packet encoding is buggy, is awaitng Async driver.

Improved Time Source[IN PROGRESS]: current version is working but should be considered a fallback. GPS based time sources or an external time keeping device should be preferred. RTC is also an option on STM32F1.

Message Authentication Code [AWAITNG_REFACTOR]: The base seed of the MAC defaults at 0 and is set to a deterministic value at linking time (based on the host and remote's id and link session id). During link time however, it is generated on a Time-based One-time Password (TOTP).
A TOTP seed is exchanged on link, which is then used to generate a token for each message. The decoding side uses the same TOTP token. The output space is only 8 bits, to optimize bandwidth. To compensate for this, the hop time is 1 second.
This project makes no claims of cryptographic security and provides the cryptographic features only for functionality purposes, such as rejecting unauthorized access and implicit addressing. Transmitted data quality is already "assured" by the Radio IC's 16 bit CRC.
Man-In-Middle attacks are very easy as data is not encrypted. There was an easy vector for denial of service, but that has been fixed. Latency is taken into account for this feature (optional).

Channel Hopping[IN PROGRESS]: The Host should hop on various channels while broadcasting, as should the remote while trying to establish a link. During link time, we can use the same time-base mechanism of the TOTP to generate a pseudo-random channel hopping. Maybe include a white-list of usable channels, which could itself be self-updating. Current version collides with packet operations on the driver and wipes the FIFO, needs improvement to work with short time frames. Waiting for async radio driver.


# Deprecated:

AVR Support: Way too much cool stuff happening for a simple Arduino to keep up with.


>> Piggyback Latency Measurement[WORKING]: There's a lot of Ack'ed packets being exchanged when linking. We track those and measure the system latency before the link is even up.
Piggy backing on linking time was producing bad values, reverted to exclusively measuring latency with Ping packets.


# Tested with: 

Maple Mini (STM32F103)

