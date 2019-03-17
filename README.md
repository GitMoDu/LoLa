# LoLa 

  

# Low Latency Radio System 

![](https://raw.githubusercontent.com/GitMoDu/LoLa/master/media/First_tests.jpg)



For Arduino compatible boards*.

(*with enough flash/memory). 
  

# This project would not be possible (and actually won't compile) without these contributions

 
Radio Driver for Si446x: https://github.com/zkemble/Si446x 

Ring Buffer: https://github.com/wizard97/Embedded_RingBuf_CPP 

Generic Callbacks: https://github.com/tomstewart89/Callback 

OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler

FastCRC with added STM32F1 support: GitMoDu/FastCRC https://github.com/GitMoDu/FastCRC
    forked from FrankBoesing/FastCRC https://github.com/FrankBoesing/FastCRC

Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git

Micro Elyptic Curve Cryptography: https://github.com/kmackay/micro-ecc

Crypto Library for modern embedded Cyphers: https://github.com/rweather/arduinolibs





# Goal for this project

Make seamlessly bi-directional communicating real-time system, for Remote Control and Robotics applications (Example: RC car and remote), with the least possible latency while still providing room for lots of "channels".

Gist of the Packet Driver: 
When receiving, it tries to throw the incoming packet to one of the registered LoLa Services. There is no callback for when no registered service wants the packet. 

The packets definitions and payload are defined in a very minimalistic way with only some configurability.
  


# Link Features

Async Driver for Si4463 [WORKING]: The radio IC this project is based around, but not limited to. Performs the packet processing in the main loop, instead of hogging the interrupt.

Message Authentication Code [WORKING]: 16 bit Modbus CRC, completely replaces the raw hardware CRC. With crypto enable, uses MAC-then-Encrypt.

Acknowledged Packets with Id [WORKING]: carrying only the original packet's header and optional id. Currently only used for establishing a link, as the Ack packets ignore the collision-avoidance setup. This way we can accuratelly measure total system latency before the link is ready.

Latency Compensation for Link[WORKING]: Using the measured latency, we use the estimated transmission time to optimize time dependent values.

ECC Public Key Exchange [WORKING] - Elyptic Curve Diffie-Hellman key exchange, performed with secp160r1. This generates a 20 byte secret key.

Encrypted Link[WORKING] – Packets encrypted with Ascon128 cypher, using 16 bytes of the shared key. To source a 16 byte IV, the partners' Ids are used, salted with the session Id.

TOTP protection [WORKING] - A TOTP seed is set using the last 4 bytes of the secret key, which is then used to generate a time based token, which is used by the cypher when encrypting/decrypting. The default hop time is 1 second.

Synchronized clock [WORKING]: when establishing a link, the Remote's clock is synced to the Host's clock. The host clock is randomized for each new link session. The clock is tuned during link time. Possible improvements: get host/remote clock delta.

Packet collision avoidance [WORKING]: with the Synchronized clock, we split a fixed period in half where the Host can only transmit during the first half and the Remote during the second half (half-duplex). Default duplex period is 10 milliseconds. Latency is taken into account for this feature (optional).

Unbuffered Output [WORKING]: Each LoLa service can handle a packet send being delayed or even failed, so we don't need to buffer outputs. IPacketSendService extends the base ILoLaService and provides overloads for extension.

Link Handshake Handling [WORKING] – Broadcast Id and find a partner. Clock is synced, Crypto tokens and basic link info is exchanged.

Link management [WORKING]: Link service establishes a link and fires events when the link is gained or lost. Keeps sending pings (with replies) to make sure the partner is still there, avoiding stealing bandwidth from user services.

Transmit Power Balancer[WORKING]: with the link up, the end-points are continuosly updated on the RSSI of the partner, and adjusts the output power conservatively.

Channel Hopping[IN PROGRESS]: The Host should hop on various channels while broadcasting, as should the remote while trying to establish a link. Currently it's using a fixed channel (average between min and max channels).
When linked, we use the TOTP mechanism to generate a pseudo-random channel hopping.

Simulated Packet Loss for Testing[IN PROGRESS]: This feature allows us to test the system in simulated bad conditions. Was reverted during last merge, needs to be reimplemented.


# Implemented services

SyncSurface Service [WORKING]: The star feature and whole reason I started this project. Synchronises an array of N blocks (32bits wide) in a differential fashion, i.e., only send over radio the changing blocks, not the whole data array. There's a 2-way protocol to ensure data integrity without compromising data update latency.


# Why not Radiohead or similar radio libraries? 

I'd like to build a real time system, biasing the implementation for low latency and code readability. Besides, it's fun. 

 


# Notable issues

It's quite a memory hog (around 1.5 kB for the example projects with only 2 sync surfaces), and ROM but there's a lot of functionality. 

 


# Future : 

Improved Time Source[IN PROGRESS]: current version is working but should be considered a fallback. GPS based time sources or an external time keeping device should be preferred. RTC is also an option on STM32F1.



# Deprecated:

AVR Support: Way too much cool stuff happening for a simple Arduino to keep up with.


# Tested with: 

Maple Mini (STM32F103)

