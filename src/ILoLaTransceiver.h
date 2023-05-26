// ILoLaTransceiver.h

#ifndef _I_LOLA_TRANSCEIVER_h
#define _I_LOLA_TRANSCEIVER_h

#include "Link\LoLaPacketDefinition.h"

class ILoLaTransceiverListener
{
public:
	/// <summary>
	/// Transceiver has a received packet to deliver,
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="receiveTimestamp">Accurate timestamp (micros()) of incoming packet start.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="rssi">Normalized RX RSSI [0:255].</param>
	/// <returns>True if packet was successfully consumed. False, try again later.</returns>
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) { return false;}

	/// <summary>
	/// Transceiver was receiveing a packet but couldn't read it or finish it.
	/// </summary>
	/// <param name="startTimestamp">Accurate timestamp (micros()) of lost packet start.</param>
	virtual void OnRxLost(const uint32_t receiveTimestamp) {}

	/// <summary>
	/// Transceiver has finished transmitting a packet.
	/// </summary>
	virtual void OnTx() {}
};

class ILoLaTransceiver
{
public:
	/// <summary>
	/// Set up the transceiver listener that will handle all packet events.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns></returns>
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) { return false; }

	/// <summary>
	/// Boot up the device and start in working mode.
	/// </summary>
	/// <returns></returns>
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
	/// Transceiver can optionally auto-return to RX mode on same channel,
	///  but Rx() will be always called after OnTx() is called from transceiver.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="channel">Normalized channel [0:255].</param>
	/// <returns>True if success.</returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) { return false; }

	/// <summary>
	/// Set to RX mode on the indicated channel.
	/// If Transceiver doesn't support channels, this can be safely ignored (even better, finalized empty).
	/// Caller is unaware of transceiver internal state.
	/// If the transceiver has already auto set to RX after a transmit, skip.
	/// If the transceiver has a fast RX_HOP mode, do a fast hop to new channel,
	///  otherwise set new channel in RX mode.
	/// </summary>
	/// <param name="channel">Abstract channel [0:255]. Must be ranged (%mod) to actual number of available channels.</param>
	virtual void Rx(const uint8_t channel) { }

	/// <summary>
	/// How long does the driver estimate transmiting will take
	///  from Tx to detected start from partner.
	/// This includes memory copies and SPI transactions and pre-ambles,
	///	 but does not include medium delay (ex: lightspeed) or internal transceiver processing. 
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) { return 0; }
};
#endif