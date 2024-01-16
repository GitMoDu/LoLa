// AbstractLoLaReceiver.h

#ifndef _ABSTRACT_LOLA_RECEIVER_
#define _ABSTRACT_LOLA_RECEIVER_

#include "AbstractLoLaSender.h"

class AbstractLoLaReceiver : public AbstractLoLaSender
{
private:
	using BaseClass = AbstractLoLaSender;

	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linking;

private:
	// The incoming plaintext content is decrypted to here, from the RawInPacket.
	uint8_t InData[LoLaPacketDefinition::GetDataSize(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)]{};

private:
	Timestamp ReceiveTimestamp{};

protected:
	/// <summary>
	/// Rolling packet counter.
	/// </summary>
	uint16_t LastValidReceivedCounter = 0;

	uint16_t ReceivedCounter = 0;

protected:
	/// <summary>
	/// Packet parser for Link implementation classes. Only fires when link is not active.
	///// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="port"></param>
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) {}

	/// <summary>
	/// Packet parser for Link implementation classes. Only fires when linking is active.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="counter"></param>
	virtual void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) {}

	/// <summary>
	/// Inform any Link class of RSSI quality and packet loss.
	/// </summary>
	/// <param name="rssi"></param>
	/// <param name="lostCount"></param>
	virtual void OnPacketReceivedOk(const uint8_t rssi, const uint8_t lostCount) {}

public:
	AbstractLoLaReceiver(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles)
		: BaseClass(scheduler, linkRegistry, encoder, transceiver, cycles)
	{}

public:
	// IPacketServiceListener
	virtual void OnLost(const uint32_t timestamp) final
	{
		OnEvent(PacketEventEnum::ReceiveRejectedTransceiver);
	}

	virtual void OnDropped(const uint32_t timestamp, const uint8_t packetSize) final
	{
		// PacketService validates for size.
		OnEvent(PacketEventEnum::ReceiveRejectedInvalid);
	}

	/// <summary>
	/// Incoming packet is been validated for:
	/// - Data integrity.
	/// - Source authenticity.
	/// - Replay/Echo denied.
	/// <param name="receiveTimestamp"></param>
	/// <param name="packetSize"></param>
	/// <param name="rssi"></param>
	virtual void OnReceived(const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		const uint8_t receivingDataSize = LoLaPacketDefinition::GetDataSize(packetSize);

		uint16_t receivingCounter = 0;
		uint8_t receivingLost = 0;

		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::AwaitingLink:
			// Update MAC without implicit addressing or token.
			// Addressing must be explicit in payload.
			if (Encoder->DecodeInPacket(RawInPacket, InData, receivingCounter, receivingDataSize))
			{
				// Save last received counter, ready for switch for next stage.
				LastValidReceivedCounter = receivingCounter;

				// Check for valid port.
				if (InData[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX] == Unlinked::PORT)
				{
					OnUnlinkedPacketReceived(receiveTimestamp,
						&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
						LoLaPacketDefinition::GetPayloadSize(packetSize),
						receivingCounter);
				}
				else
				{
					OnEvent(PacketEventEnum::ReceiveRejectedHeader);
				}
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
			}
			break;
		case LinkStageEnum::Linking:
			// Update MAC with implicit addressing but without token.
			if (Encoder->DecodeInPacket(RawInPacket, InData, 0, receivingCounter, receivingDataSize))
			{
				// Validate counter and check for valid port.
				if (ValidateCounter(receivingCounter, receivingLost)
					&& InData[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX] == Linking::PORT)
				{
					// Counter accepted, update local tracker.
					LastValidReceivedCounter = receivingCounter;

					OnLinkingPacketReceived(receiveTimestamp,
						&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
						LoLaPacketDefinition::GetPayloadSize(packetSize),
						receivingCounter);

					OnPacketReceivedOk(rssi, receivingLost);
				}
				else
				{
					OnEvent(PacketEventEnum::ReceiveRejectedHeader);
				}
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
			}
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestamp(ReceiveTimestamp);
			ReceiveTimestamp.ShiftSubSeconds(-(micros() - receiveTimestamp));
			if (Encoder->DecodeInPacket(RawInPacket, InData, ReceiveTimestamp.Seconds, receivingCounter, receivingDataSize))
			{
				// Validate counter and check for valid port.
				if (ValidateCounter(receivingCounter, receivingLost))
				{
					// Counter accepted, update local tracker.
					LastValidReceivedCounter = receivingCounter;
					ReceivedCounter++;

					Registry->NotifyPacketListener(receiveTimestamp,
						&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
						LoLaPacketDefinition::GetPayloadSize(packetSize),
						InData[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX]);

					OnPacketReceivedOk(rssi, receivingLost);
				}
				else
				{
					OnEvent(PacketEventEnum::ReceiveRejectedHeader);
				}
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
			}
			break;
		default:
			break;
		}
	}

protected:
	const bool MockReceiveFailPacket(const uint32_t receiveTimestamp, const uint8_t* data, const uint8_t payloadSize)
	{
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		uint16_t receivingCounter = 0;

		SyncClock.GetTimestamp(ReceiveTimestamp);
		ReceiveTimestamp.ShiftSubSeconds(-(micros() - receiveTimestamp));

		// (Fail to) Decrypt packet with token based on time.
		return Encoder->DecodeInPacket(data, RawInPacket, ReceiveTimestamp.Seconds, receivingCounter, LoLaPacketDefinition::GetDataSize(packetSize));
	}

private:
	const bool ValidateCounter(const uint16_t counter, uint8_t& receiveLost)
	{
		const uint16_t counterRoll = counter - LastValidReceivedCounter;
		if (counterRoll < LoLaLinkDefinition::ROLLING_COUNTER_ERROR)
		{
			receiveLost = counterRoll - 1;
			return true;
		}
		else
		{
			receiveLost = 0;
			return false;
		}
	}
};
#endif