// PIMLoLaPacketDriver.h
// For use with Pulse Interval Modulator (https://github.com/GitMoDu/PulseIntervalModulator).

#ifndef _LOLA_PIM_PACKET_DRIVER_h
#define _LOLA_PIM_PACKET_DRIVER_h


#include <ILoLaPacketDriver.h>

#define PIM_NO_CHECKS
#include <PulsePacketTaskDriver.h>


template<const uint8_t MaxPayloadSize>
class PIMLoLaPacketDriver
	: public PulsePacketTaskDriver<LoLaPacketDefinition::GetTotalSize(MaxPayloadSize)>
	, public virtual ILoLaPacketDriver
{
private:
	using BaseClass = PulsePacketTaskDriver<LoLaPacketDefinition::GetTotalSize(MaxPayloadSize)>;

	using BaseClass::IncomingPacket;
	using BaseClass::Start;
	using BaseClass::Stop;
	using BaseClass::CanSend;
	using BaseClass::SendPacket;

	ILoLaPacketDriverListener* PacketListener = nullptr;

public:
#if defined(ARDUINO_ARCH_AVR)
	PIMLoLaPacketDriver(Scheduler& scheduler, const uint8_t readPin, const uint8_t writePin)
		: BaseClass(scheduler, readPin, writePin)
#elif defined(ARDUINO_ARCH_STM32F1)
	PIMLoLaPacketDriver(Scheduler& scheduler, const uint8_t readPin, const uint8_t writePin, const uint8_t timerIndex, const uint8_t timerChannel)
		: BaseClass(scheduler, readPin, writePin, timerIndex, timerChannel)
#endif
		, ILoLaPacketDriver()
	{}

public:
	// PIM Driver overrides.
	virtual void OnDriverPacketReceived(const uint32_t startTimestamp, const uint8_t packetSize) final
	{
		PacketListener->OnReceived(IncomingPacket, startTimestamp, packetSize, UINT8_MAX);
	}

	virtual void OnDriverPacketLost(const uint32_t startTimestamp) final
	{
		PacketListener->OnReceiveLost(startTimestamp);
	}

	virtual void OnDriverPacketSent() final
	{
		PacketListener->OnSent(true);
	}

	// ILoLaPacketDriver overrides.
	virtual const bool DriverSetup(ILoLaPacketDriverListener* packetListener) final
	{
		PacketListener = packetListener;

		return PacketListener != nullptr;
	}

	virtual const bool DriverStart() final
	{
		Start();

		return PacketListener != nullptr;
	}

	virtual const bool DriverStop() final
	{
		Stop();

		return true;
	}

	virtual const bool DriverCanTransmit() final
	{
		// Make sure we wait at least a bit over a pre amble before sending again.
		return CanSend();
	}

	virtual void DriverReceive(const uint8_t channel) final {}

	virtual const bool DriverTransmit(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		// Packet copied, freeing the output buffer.
		return SendPacket(data, packetSize);
	}

	virtual const uint8_t GetLastRssiIndicator() final
	{
		// As far as the PIM driver goes, all reception is perfect reception.
		return UINT8_MAX;
	}

	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		// According to PIM specs, around ~500us per byte.
		return ((uint32_t)500) * packetSize;
	}
};
#endif