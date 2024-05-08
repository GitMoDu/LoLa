// ILoLaTransceiver.h

#ifndef _I_LOLA_TRANSCEIVER_h
#define _I_LOLA_TRANSCEIVER_h

#include "Link\LoLaPacketDefinition.h"

class ILoLaTransceiverListener
{
public:
	/// <summary>
	/// Transceiver has a received packet to deliver.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="receiveTimestamp">Accurate timestamp (micros()) of incoming packet start.</param>
	/// <param name="packetSize">[MIN_PACKET_SIZE;MAX_PACKET_TOTAL_SIZE]</param>
	/// <param name="rssi">Normalized RX RSSI [0;255].</param>
	/// <returns>True if packet was successfully consumed. False, try again later.</returns>
	virtual const bool OnRx(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssi) { return false; }

	/// <summary>
	/// Transceiver was receiving a packet but couldn't read it or finish it.
	/// </summary>
	/// <param name="startTimestamp">Accurate timestamp (micros()) of lost packet start.</param>
	virtual void OnRxLost(const uint32_t timestamp) {}

	/// <summary>
	/// Transceiver has finished transmitting a packet.
	/// </summary>
	virtual void OnTx() {}
};

class ILoLaTransceiver
{
public:
	/// <summary>
	/// Maps the abstract channel to the limited physical channels.
	/// </summary>
	/// <typeparam name="ChannelCount">[1;255]</typeparam>
	/// <param name="abstractChannel">[0;UINT8_MAX</param>
	/// <returns>Index of real channel to use.</returns>
	template<const uint8_t ChannelCount>
	static constexpr uint8_t GetRealChannel(const uint8_t abstractChannel)
	{
		return ((uint16_t)abstractChannel * (ChannelCount - 1)) / UINT8_MAX;
	}

public:
	/// <summary>
	/// Set up the transceiver listener that will handle all packet events.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True on success.</returns>
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) { return false; }

	/// <summary>
	/// Boot up the device and start in working mode.
	/// </summary>
	/// <returns>True on success.</returns>
	virtual const bool Start() { return false; }

	/// <summary>
	/// Stop the device and turn off power if possible.
	/// </summary>
	/// <returns>True if transceiver was running.</returns>
	virtual const bool Stop() { return false; }

	/// <summary>
	/// Inform the caller if the transceiver can currently perform a Tx.
	/// </summary>
	/// <returns>True if yes.</returns>
	virtual const bool TxAvailable() { return false; }

	/// <summary>
	/// Tx/Transmit a packet at the indicated channel.
	/// After OnTx(), Rx() or Tx() will be always called.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="channel">Normalized channel [0:255].</param>
	/// <returns>True if success.</returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) { return false; }

	/// <summary>
	/// Set to RX mode on the indicated channel.
	/// If Transceiver doesn't support channels, this can be safely ignored.
	/// Caller is unaware of transceiver internal state.
	/// If the transceiver has already auto set to RX after a transmit, skip.
	/// If the transceiver has a fast RX_HOP mode, do a fast hop to new channel,
	///  otherwise set new channel in RX mode.
	/// </summary>
	/// <param name="channel">Abstract channel [0:255].</param>
	virtual void Rx(const uint8_t channel) { }

	/// <summary>
	/// How long transmiting will take from Tx request to start In-Air.
	/// This includes memory copies and SPI transactions and pre-ambles,
	///	 but does not include medium delay (ex: lightspeed). 
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns>Duration In microseconds.</returns>
	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) { return 0; }

	/// <summary>
	/// How long transmitting will take from start to end of In-Air.
	/// In microseconds.
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns>Duration In microseconds.</returns>
	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) { return 0; }

	/// <summary>
	///	Unique id for Transceiver type.
	/// Should also change with configuration parameters such as:
	/// - Channel count.
	/// - Baud-rate.
	/// - Modulation.
	/// - Encoding.
	/// - Framing.
	/// - CRC.
	/// </summary>
	/// <returns>32 Bit unique id.</returns>
	virtual const uint32_t GetTransceiverCode() { return 0; }

	/// <summary>
	/// Optional interface used for debug/display purposes only.
	/// Not required for operation.
	/// </summary>
	/// <returns>How many channels does the transceiver support.</returns>
	virtual const uint8_t GetChannelCount() { return 0; }

	/// <summary>
	/// Optional interface used for debug/display purposes only.
	/// Not required for operation.
	/// </summary>
	/// <returns></returns>
	virtual const uint8_t GetCurrentChannel() { return 0; }
};
#endif