// ILoLaLink.h

#ifndef _I_LOLA_LINK_h
#define _I_LOLA_LINK_h


class ILinkListener
{
public:
	/// <summary>
	/// Notifies listener that link has been acquired/lost.
	/// </summary>
	/// <param name="hasLink">Same value as ILoLaLink::HasLink()</param>
	virtual void OnLinkStateUpdated(const bool hasLink) {}
};

class ILinkPacketListener
{
public:
	/// <summary>
	/// 
	/// </summary>
	/// <param name="data"></param>
	/// <param name="startTimestamp"></param>
	/// <param name="port"></param>
	/// <param name="payloadSize"></param>
	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) {}
};

class ILoLaLink
{
public:
	/// <summary>
	/// Start link. Wake up PHY.
	/// </summary>
	/// <returns></returns>
	virtual const bool Start() { return false; }

	/// <summary>
	/// Stop link. Disable PHY.
	/// </summary>
	/// <returns></returns>
	virtual const bool Stop() { return false; }

	/// <summary>
	/// Do we currently have a Link?
	/// </summary>
	/// <returns></returns>
	virtual const bool HasLink() { return false; }

	/// <summary>
	/// Get the current link duration in seconds.
	/// </summary>
	virtual const uint32_t GetLinkDuration() { return 0; }

	virtual void DebugClock() { }

	/// <summary>
	/// Can a Link time packet be sent now?
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns></returns>
	virtual const bool CanSendPacket(const uint8_t payloadSize) { return false; }

	/// <summary>
	/// Send packet through link using transceiver.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>True on successfull transmission.</returns>
	virtual const bool SendPacket(const uint8_t* data, const uint8_t payloadSize) { return false; }

	/// <summary>
	/// Register link status listener.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True if success. False if no more slots are available.</returns>
	virtual const bool RegisterLinkListener(ILinkListener* listener) { return false; }

	/// <summary>
	/// 
	/// </summary>
	/// <param name="listener"></param>
	/// <param name="port">Port number to register. Note that a port (upmost) may be reserved for Link.</param>
	/// <returns>True if success. False if no more slots are available or port is reserved.</returns>
	virtual const bool RegisterPacketReceiver(ILinkPacketListener* listener, const uint8_t port) { return false; }
};
#endif