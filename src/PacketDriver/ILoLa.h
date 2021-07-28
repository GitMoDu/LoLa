// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <stdint.h>

class ILoLaLinkListener
{
public:
	void OnLinkStateUpdated(const bool linked) {}
};

class ILoLaLinkPacketListener : public virtual ILoLaLinkListener
{
public:
	// Return true if driver should return an ack.
	virtual const bool OnPacketReceived(uint8_t* data, const uint32_t startTimestamp, const uint8_t header, const uint8_t packetSize) { return false; }
	virtual void OnPacketAckReceived(const uint32_t startTimestamp, const uint8_t header) { }
};

class ILoLaPacketListener
{
public:
	// Return true when an Ack should be replied back.
	virtual const bool OnPacketReceived(uint8_t* data, const uint32_t startTimestamp, const uint8_t packetSize) { return false; }

	virtual void OnPacketLost(const uint32_t startTimestamp) {}

	virtual void OnPacketSent() {}
};

class ILoLaPacketDriver
{
public:
	virtual const bool DriverSetup(ILoLaPacketListener* packetListener) { return false; }

	virtual void DriverStart() {}

	virtual void DriverStop() {}

	virtual const bool DriverCanTransmit() { return false; }

	virtual const bool DriverTransmit(uint8_t* data, const uint8_t length) { return false; }

	virtual void DriverSetChannel(const uint8_t channel) { }

	// [0:255] How good was the last reception?
	virtual const uint8_t GetLastRssiIndicator() { return 0; }

	// How long should the driver wait for a completed transmission, before timing out?
	virtual const uint32_t GetTransmitTimeoutMicros(const uint8_t packetSize) { return 0; }

	// How long should the driver wait for a reply, before timing out?
	virtual const uint32_t GetReplyTimeoutMicros(const uint8_t packetSize) { return 0; }
};

class ILoLaLink
{
public:
	virtual const bool SendPacket(uint8_t* payload, const uint8_t payloadSize, const uint8_t header, const bool hasAck) { return false; }

	virtual const bool RegisterLinkListener(ILoLaLinkListener* listener) { return false; }
	virtual const bool RegisterLinkPacketListener(const uint8_t header, ILoLaLinkPacketListener* listener) { return false; }
};


#endif