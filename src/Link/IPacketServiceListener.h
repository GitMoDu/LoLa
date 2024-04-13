// IPacketServiceListener.h

#ifndef _I_PACKET_SERVICE_LISTENER_h
#define _I_PACKET_SERVICE_LISTENER_h

#include <stdint.h>

class IPacketServiceListener
{
public:
	/// <summary>
	/// Possible results for the send request.
	/// </summary>
	enum SendResultEnum
	{
		Success,
		SendTimeout,
		SendCollision,
		Error
	};

public:
	/// <summary>
	/// Driver set to RX is always initiated by the packet service,
	///  so the ServiceListener must provide the channel for RX setting.
	/// </summary>
	/// <returns>The current channel to set to.</returns>
	virtual const uint8_t GetRxChannel() { return 0; }

	/// <summary>
	/// The Packet Service has a received packet ready to consume.
	/// </summary>
	/// <param name="receiveTimestamp">micros() timestamp of packet start.</param>
	/// <param name="data">Raw packet data is provided separately, to avoid a repeating field.</param>
	/// <param name="packetSize"></param>
	virtual void OnReceived(const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) { }

	/// <summary>
	/// The Physical Driver failed to receive an integral packet.
	/// </summary>
	/// <param name="timestamp"></param>
	virtual void OnLost(const uint32_t timestamp) { }

	/// <summary>
	/// Optional callback.
	/// Lets the Sender know the transmission is complete or failed.
	/// </summary>
	/// <param name="result"></param>
	virtual void OnSendComplete(const SendResultEnum result) {}
};
#endif