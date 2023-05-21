// ILoLaRxTxDriver.h

#ifndef _I_LOLA_RX_TX_DRIVER_h
#define _I_LOLA_RX_TX_DRIVER_h

#include "Link\LoLaPacketDefinition.h"

class ILoLaRxTxListener
{
public:
	/// <summary>
	/// Driver has a received packet to deliver,
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="receiveTimestamp">Accurate timestamp (micros()) of incoming packet start.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="rssi">Normalized RX RSSI [0:255].</param>
	/// <returns>True if packet was successfully consumed. False, try again later.</returns>
	virtual const bool OnReceived(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) { return false;}

	/// <summary>
	/// Transceiver was receiveing a packet but couldn't read it or finish it.
	/// </summary>
	/// <param name="startTimestamp">Accurate timestamp (micros()) of lost packet start.</param>
	virtual void OnReceiveLost(const uint32_t receiveTimestamp) {}

	/// <summary>
	/// Transceiver has finished transmitting a packet.
	/// </summary>
	/// <param name="success">True if success.</param>
	virtual void OnSent(const bool success) {}
};

class ILoLaRxTxDriver
{
public:
	/// <summary>
	/// Set up the transceiver listener that will handle all packet events.
	/// </summary>
	/// <param name="packetListener"></param>
	/// <returns></returns>
	virtual const bool DriverSetupListener(ILoLaRxTxListener* packetListener) { return false; }

	/// <summary>
	/// Boot up the device and start in working mode.
	/// </summary>
	/// <returns></returns>
	virtual const bool DriverStart() { return false; }

	/// <summary>
	/// Stop the device and turn off power if possible.
	/// </summary>
	/// <returns>True if transceiver was running.</returns>
	virtual const bool DriverStop() { return false; }

	/// <summary>
	/// Inform the caller if the transceiver can currently perform a DriverTx.
	/// </summary>
	/// <returns>True if yes.</returns>
	virtual const bool DriverCanTransmit() { return false; }

	/// <summary>
	/// TX a packet at the indicated channel.
	/// Driver can optionally auto-return to RX mode on same channel,
	///  but DriverRx will be always called after OnSent is called from Driver.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="channel">Normalized channel [0:255].</param>
	/// <returns>True if success.</returns>
	virtual const bool DriverTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) { return false; }

	/// <summary>
	/// Set to RX mode on the indicated channel.
	/// If RxTXDriver doesn't support channel, can be safely ignored (even better, finalized empty).
	/// Caller is unaware of driver internal state.
	/// If the driver has already auto set to RX after a transmit, skip.
	/// If the driver has a fast RX_HOP mode, do a fast hop to new channel,
	///  otherwise set new channel in RX mode.
	/// </summary>
	/// <param name="channel">Normalized channel [0:255].</param>
	virtual void DriverRx(const uint8_t channel) { }

	/// <summary>
	/// How long does the driver estimate transmiting will take
	///  from DriverTx to detected start from partner.
	/// This includes memory copies and SPI transactions and pre-ambles,
	///	 but does not include medium delay (ex: lightspeed) or internal transceiver processing. 
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) { return 0; }

	///// <summary>
	///// How long does the driver estimate switching Rx channel will take?
	///// </summary>
	///// <returns>The expected hop duration in microseconds.</returns>
	//virtual const uint32_t GetHopDurationMicros() { return 0; }
};
#endif