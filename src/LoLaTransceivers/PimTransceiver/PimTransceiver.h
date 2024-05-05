// PimTransceiver.h
// For use with Pulse Interval Modulator (https://github.com/GitMoDu/PulseIntervalModulator).

#ifndef _PIM_TRANSCEIVER_h
#define _PIM_TRANSCEIVER_h

#include "../ILoLaTransceiver.h"

#define PIM_NO_CHECKS
#include <PulsePacketTaskDriver.h>


template<const uint8_t IdCode = 0>
class PimTransceiver
	: public PulsePacketTaskDriver<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE>
	, public virtual ILoLaTransceiver
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0x9114;

private:
	using BaseClass = PulsePacketTaskDriver<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE>;

	using BaseClass::IncomingPacket;
	using BaseClass::Start;
	using BaseClass::Stop;
	using BaseClass::CanSend;
	using BaseClass::SendPacket;

	ILoLaTransceiverListener* Listener = nullptr;

public:
#if defined(ARDUINO_ARCH_AVR)
	PimTransceiver(Scheduler& scheduler, const uint8_t readPin, const uint8_t writePin)
		: BaseClass(scheduler, readPin, writePin)
#elif defined(ARDUINO_ARCH_STM32F1)
	PimTransceiver(Scheduler& scheduler, const uint8_t readPin, const uint8_t writePin, const uint8_t timerIndex, const uint8_t timerChannel)
		: BaseClass(scheduler, readPin, writePin, timerIndex, timerChannel)
#endif
		, ILoLaTransceiver()
	{}

public:
	// PIM Driver overrides.
	virtual void OnDriverPacketReceived(const uint32_t receiveTimestamp, const uint8_t packetSize) final
	{
		Listener->OnRx(IncomingPacket, receiveTimestamp, packetSize, UINT8_MAX);
	}

	virtual void OnDriverPacketLost(const uint32_t receiveTimestamp) final
	{
		Listener->OnRxLost(receiveTimestamp);
	}

	virtual void OnDriverPacketSent() final
	{
		Listener->OnTx();
	}

	// ILoLaPacketDriver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		BaseClass::Start();

		return false;
	}

	virtual const bool Stop() final
	{
		BaseClass::Stop();

		return false;
	}

	virtual const bool TxAvailable() final
	{
		// Make sure we wait at least a bit over a pre amble before sending again.
		return CanSend();
	}

	virtual void Rx(const uint8_t channel) final
	{

	}

	virtual const uint32_t GetTransceiverCode()
	{
		return (uint32_t)TRANSCEIVER_ID
			//+ (uint32_t)baudRateCode << 16 //TODO:
			| (uint32_t)IdCode << 24;
	}

	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
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