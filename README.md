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

FastCRC: FrankBoesing/FastCRC https://github.com/FrankBoesing/FastCRC

Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git

Micro Elyptic Curve Cryptography: https://github.com/kmackay/micro-ecc

Crypto Library for modern embedded Cyphers: https://github.com/rweather/arduinolibs





# Goal for this project

Make seamlessly bi-directional communicating real-time system, for Remote Control and Robotics applications (Example: RC car and remote), with the least possible latency while still providing room for lots of "channels".

Gist of the Packet Driver: 
When receiving, it tries to throw the incoming packet to one of the registered LoLa Services. There is no callback for when no registered service wants the packet. 

The packets definitions and payload are defined in a very minimalistic way with only some configurability.
  


# Link Features

Unbuffered Output [WORKING]: Each LoLa service can handle a packet send being delayed or even failed, so we don't need to buffer outputs. IPacketSendService extends the base ILoLaService and provides overloads for extension.

Async Driver for Si4463 [WORKING]: The radio IC this project is based around, but not limited to. Performs the packet processing in the main loop, instead of hogging the interrupt.

Message Authentication Code [WORKING]: Blake2s hash, completely replaces the raw hardware CRC. With crypto enable, uses Encrypt-then-Mac.

Acknowledged Packets with Id [WORKING]: carrying only the original packet's header and optional id. Mostly used for measuring total system latency and linking status progression.

Latency Compensation for Link[WORKING]: Using the measured latency, we use the estimated transmission time to optimize time dependent values.

ECC Public Key Exchange [WORKING] - Elyptic Curve Diffie-Hellman key exchange, performed with secp160r1. This generates a 20 byte secret key.

Encrypted Link[WORKING] – Packets encrypted with Ascon128 cypher, using 16 bytes of the shared key. To source a 16 byte IV, the partners' Ids are used, salted with the session Id.

TOTP protection [NEEDS_TESTING] - A TOTP seed is set using expanded bytes from secret key, which is then used to generate a time based token, which is used by the cypher when encrypting/decrypting. The default hop time is 1 second.

Synchronized clock [WORKING]: when establishing a link, the Remote's clock is synced to the Host's clock. The host clock is randomized for each new link session. The clock is tuned during link time. Possible improvements: get host/remote clock delta.

Packet collision avoidance [WORKING]: with the Synchronized clock, we split a fixed period in half where the Host can only transmit during the first half and the Remote during the second half (half-duplex). Default duplex period is 10 milliseconds. Latency is taken into account for this feature (optional).

Link Handshake Handling [WORKING] – Broadcast Id and find a partner. Clock is synced, Crypto tokens and basic link info is exchanged.

Link management [WORKING]: Link service establishes a link and fires events when the link is gained or lost. Keeps sending pings (with replies) to make sure the partner is still there, avoiding stealing bandwidth from user services.

Transmit Power Balancer[WORKING/NEEDS TUNNING]: with the link up, the end-points are continuosly updated on the RSSI of the partner, and adjusts the output power conservatively. Currently it's too jumpy and not conservative enough.

Channel Hopping[NEEDS_TESTING]: The Host should hop on various channels while broadcasting, as should the remote while trying to establish a link. When linked, we use the TOTP mechanism to generate a pseudo-random channel hopping.

Simulated Packet Loss for Testing[IN PROGRESS]: This feature allows us to test the system in simulated bad conditions. Was reverted during last merge, needs to be reimplemented.

Reliable Time Source[WORKING]: RTC is working as an option on STM32F1. A rtc-tick-trained timer provides sub-second precision.

Crypto key derivation [WORKING]: currently using a custom key derivation, should be replaced with standard KDF.


# Implemented services

SyncSurface Service [WORKING]: The star feature and whole reason I started this project. Synchronises an array of N blocks (32bits wide) in a differential fashion, i.e., only send over radio the changing blocks, not the whole data array. There's a 2-way protocol to ensure data integrity without compromising data update latency.


# Why not Radiohead or similar radio libraries? 

I'd like to build a real time system, biasing the implementation for low latency and code readability. Besides, it's fun. 

 

# Notable issues

It's quite a memory hog (around 1.5 kB for the example projects with only 2 sync surfaces), and ROM but there's a lot of functionality. 

 


# Future : 

Drunk rebuild. Don't ask, I wasn't happy with a lot of stuff, so I changed a lot of stuff. Now nothing works, but it's fine, it will.

Support for other radio drivers [WORK_IN_PROGRESS]: NRF24L01, Ti 

SyncSurface Service upgrade to 64 bit blocks: should allow for easier 64 bit native values and more bandwidth efficient sync.

Slave Device: turn a small micro


# Deprecated:

AVR Support: Way too much cool stuff happening for a simple Arduino to keep up with.


# Tested with: 

Maple Mini (STM32F103)

