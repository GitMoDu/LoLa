// LoLaPIMPacketDriver.h
// For use with Pulse Interval Modulator (https://github.com/GitMoDu/PulseIntervalModulator).

#ifndef _LOLA_PIM_PACKET_DRIVER_h
#define _LOLA_PIM_PACKET_DRIVER_h


#include <PacketDriver\ILoLa.h>

#define PIM_NO_CHECKS
#include <PulsePacketTaskDriver.h>


template<const uint8_t MaxPacketSize, const uint8_t ReadPin, const uint8_t WritePin, const uint32_t MinimumSilenceInterval = 20000>
class LoLaPIMPacketDriver
	: public PulsePacketTaskDriver<MaxPacketSize, ReadPin, WritePin, MinimumSilenceInterval>
	, public virtual ILoLaPacketDriver
{
private:
	ILoLaPacketListener* PacketListener = nullptr;

	using PulsePacketTaskDriver<MaxPacketSize, ReadPin, WritePin, MinimumSilenceInterval>::IncomingPacket;
	using PulsePacketTaskDriver<MaxPacketSize, ReadPin, WritePin, MinimumSilenceInterval>::Start;
	using PulsePacketTaskDriver<MaxPacketSize, ReadPin, WritePin, MinimumSilenceInterval>::Stop;

public:
	LoLaPIMPacketDriver(Scheduler* scheduler)
		: PulsePacketTaskDriver<MaxPacketSize, ReadPin, WritePin, MinimumSilenceInterval>(scheduler)
		, ILoLaPacketDriver()
	{
	}

public:
	// PIM Driver overrides.
	virtual void OnDriverPacketReceived(const uint32_t startTimestamp, const uint8_t packetSize)
	{
		PacketListener->OnPacketReceived(IncomingPacket, startTimestamp, packetSize);
	}

	virtual void OnDriverPacketLost(const uint32_t startTimestamp)
	{
		PacketListener->OnPacketLost(startTimestamp);
	}

	virtual void OnDriverPacketSent()
	{
		PacketListener->OnPacketSent();
	}

	// ILoLaPacketDriver overrides.
	virtual const bool DriverSetup(ILoLaPacketListener* packetListener)
	{
		PacketListener = packetListener;

		return PacketListener != nullptr;
	}

	virtual void DriverStart()
	{
		Start();
	}

	virtual void DriverStop()
	{
		Stop();
	}

	virtual const bool DriverCanTransmit()
	{
		return CanSend();
	}

	virtual const bool DriverTransmit(uint8_t* data, const uint8_t length)
	{
		if (CanSend())
		{
			// Packet it copied to output buffer, freeing the packet buffer.
			SendPacket(data, length);
			return true;
		}
		else
		{
			return false;
		}
	}

	virtual const uint8_t GetLastRssiIndicator()
	{
		// As far as the PIM driver goes, all reception is perfect reception.
		return UINT8_MAX;
	}

	virtual const uint32_t GetTransmitTimeoutMicros(const uint8_t packetSize)
	{
		// According to PIM specs, around ~1ms per byte.
		return (uint32_t)packetSize * 500;
	}

	virtual const uint32_t GetReplyTimeoutMicros(const uint8_t packetSize)
	{
		// A bit longe than expected, just for margin.
		return GetTransmitTimeoutMicros(packetSize + 5);
	}
};
#endif