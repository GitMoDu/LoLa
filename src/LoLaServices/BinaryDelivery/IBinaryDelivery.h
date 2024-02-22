// IBinaryDelivery.h

#ifndef _IBINARY_DELIVERY_h
#define _IBINARY_DELIVERY_h

class IBinaryDelivery
{
public:
	static const uint8_t MaxChunkSize = 16;

public:
	/// <summary>
	/// Allows a single binary service to serve up to 255 individual binaries, shared amongst Sender and Receiver.
	/// </summary>
	/// <returns>The binary identifier.</returns>
	virtual const uint8_t GetId() { return 0; }
};

class IBinarySender : public virtual IBinaryDelivery
{
public:
	/// <summary>
	/// </summary>
	/// <returns>Total size of binary, in bytes.</returns>
	virtual const uint32_t GetDataSize() { return 0; }

	/// <summary>
	/// Callback that occurs when the binary delivery is complete.
	/// </summary>
	/// <returns>True if sending was successful.</returns>
	virtual const bool OnFinishedSending() {}

	/// <summary>
	/// </summary>
	/// <param name="address">Data offset address.</param>
	/// <returns>Data byte at address.</returns>
	virtual const uint8_t GetByte(const uint32_t address) { return 0; }

	///// <summary>
	///// </summary>
	///// <param name="address">Data offset address.</param>
	///// <returns>Data uint32_t at address.</returns>
	//virtual const uint32_t Get32(const uint32_t address) { return 0; }
};

class IBinaryReceiver : public virtual IBinaryDelivery
{
public:
	virtual const uint8_t SetByte(const uint32_t address) { return 0; }

	/// <summary>
	/// Callback that occurs when binary is up for delivery.
	/// </summary>
	/// <param name="dataSize">The data size in bytes.</param>
	/// <returns>True if the data size is within what the receiver can handle.</returns>
	virtual const bool OnStartReceiving(const uint32_t dataSize) { return 0; }

	/// <summary>
	/// Callback that occurs when the binary delivery is complete.
	/// //TODO: Provide a inbuilt CRC?
	/// </summary>
	virtual void OnFinishedReceiving() {}
};


class IBinaryDeliveryService
{
public:
	virtual const bool RequestSendBinary(IBinarySender* sendRequest) { return false; }
	virtual const bool AttachReceiver(IBinaryReceiver* requestReceiver) { return false; }
};
#endif