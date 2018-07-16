# LoLa 

  

# Low Latency Radio System 

![](https://raw.githubusercontent.com/GitMoDu/LoLa/master/media/First_tests.jpg)



For Arduino compatible boards. 
  

# THIS PROJECT IS A WORKING IN PROGRESS, DO NOT EXPECT ANYTHING TO WORK OUT OF THE BOX 

  

This project would not be possible (and actually won't compile) without the contributions for: 


Radio Driver for Si446x: https://github.com/zkemble/Si446x 

Ring Buffer: https://github.com/wizard97/Embedded_RingBuf_CPP 

Generic Callbacks: https://github.com/tomstewart89/Callback 

OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler

Tiny CRC calculator: https://github.com/FrankBoesing/FastCRC.git

Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git 

 

  

Goal for this project: 


Make seamlessly bi-directional communicating real-time system, for Remote Control and Robotics applications (Example: RC car and remote), with the least possible latency. 

To achieve this, every real time action is based on interrupt events and reactive Tasks that do the necessary work (hurry) and go back to sleep. 

 


Why not Radiohead or similar radio libraries? 

I'd like to build a real time system, biasing the implementation for low latency and code readability. Besides, it's fun. 

 

Gist of the inner working: 

The packet driver is not a generic packet handler: it only knows how to receive and send packets. When receiving, it tries to throw the incoming packet to one of the registered LoLa Service. There is no callback for when no service wants the packet. 

The packets definitions and payload are defined in a very minimalistic way with only some configurability. 

 


Features: 

Simple Acknowledged Packets with Id: acks only carry the original packet's id. 

Packet collision avoidance: tracking time elapsed after the last receive/send and rejecting a send when a packet is incoming. 

No buffer outputs: Each LoLa service can handle a packet send being delayed or even failed, so we don't need to buffer outputs. 

Connection manager:  Tries to establish connection and measure properties, while firing events to notify the services. 

Connection diagnostics: can fire events when the connection is lost or keep sending pings (with Ack) to make sure the partner is still there. 

SyncSurface Service: synchronises an array of N blocks (32bits wide) in a differential fashion, i.e., only send over radio the changing blocks, not the whole data array. 

  

Notable issues: 

It's quite a memory hog (around 1,5 kB for the example projects with only 2 sync surfaces), and ROM but there's a lot of functionality. 

 

 

Future : 

Improved Packet Collision avoidance – Using the measured real system latency, instead of hard coded value. Work in progress. 

Connection Handling – Broadcast, send challenge response and start session. Work in progress. 

Auto channel selection for remote. 

Channel hoping. 

Message Authentication Code (MAC) - We can't have proper encryption with these little micros, but we should at least be able to reject unauthorized access. 

 

Tested with: 

  

ATmega328p @ 8 MHz 

ATmega328p @ 16 MHz 

Maple Mini clone 
