// AbstractLoLaReceiver.h

#ifndef _ABSTRACT_LOLA_RECEIVER_
#define _ABSTRACT_LOLA_RECEIVER_

#include "AbstractLoLaSender.h"


template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaReceiver : public AbstractLoLaSender<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaSender<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;

	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;

protected:
	using BaseClass::SyncClock;
	using BaseClass::Encoder;

	using BaseClass::RawInPacket;
	using BaseClass::ZeroTimestamp;
	using BaseClass::InData;

	using BaseClass::PacketService;

	using BaseClass::LinkStage;


	using BaseClass::OnEvent;
	using BaseClass::NotifyPacketReceived;

protected:
	/// <summary>
	/// Rolling packet counter.
	/// </summary>
	uint8_t LastValidReceivedCounter = 0;

	/// <summary>
	/// RSSI (Received Signal Strenght Indicator) for Link quality evaluation.
	/// Unitless scale -> [0, UINT8_MAX].
	/// </summary>
	uint8_t LastReceivedRssi = 0;

private:
	/// <summary>
	/// Timestamp in millis().
	/// </summary>
	uint32_t LastValidReceived = 0;


private:
	Timestamp ReceiveTimestamp{};

	uint8_t ReceivingCounter = 0;

protected:
	/// <summary>
	/// Packet parser for Link implementation classes. Only fires when link is not active.
	///// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="port"></param>
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) {}

	virtual void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) {}

	/// <summary>
	/// Inform any Link class of packet loss detection.
	/// </summary>
	/// <param name="lostCount"></param>
	/// <returns></returns>
	virtual void OnReceiveLossDetected(const uint8_t lostCount) {}

public:
	AbstractLoLaReceiver(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles)
		: BaseClass(scheduler, encoder, transceiver, cycles)
	{}

public:
	// IPacketServiceListener
	virtual void OnLost(const uint32_t timestamp) final
	{
		OnEvent(PacketEventEnum::ReceiveRejectedTransceiver);
	}

	virtual void OnReceived(const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE || packetSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			OnEvent(PacketEventEnum::ReceiveRejectedInvalid);
			return;
		}

		const uint8_t receivingDataSize = LoLaPacketDefinition::GetDataSize(packetSize);

		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			return;
		case LinkStageEnum::AwaitingLink:
			// Update MAC without implicit addressing or token.
			// Addressing must be explicit in payload.
			if (Encoder->DecodeInPacket(RawInPacket, InData, ReceivingCounter, receivingDataSize))
			{
				// Save last received counter, ready for switch for next stage.
				LastValidReceivedCounter = ReceivingCounter;
				LastReceivedRssi = rssi;
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
				return;
			}
			break;
		case LinkStageEnum::Linking:
			// Update MAC with implicit addressing but without token.
			if (Encoder->DecodeInPacket(RawInPacket, InData, ZeroTimestamp, ReceivingCounter, receivingDataSize))
			{
				if (ValidateCounter(ReceivingCounter))
				{
					// Counter accepted, update local tracker.
					LastValidReceivedCounter = ReceivingCounter;
					LastReceivedRssi = rssi;
				}
				else
				{
					// Only downgrade link quality, 
					// to slightly prevent denial of service by RSSI lying.
					if (rssi < LastReceivedRssi)
					{
						LastReceivedRssi = rssi;
					}

					OnEvent(PacketEventEnum::ReceiveRejectedCounter);
					return;
				}
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
				return;
			}
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestamp(ReceiveTimestamp);
			ReceiveTimestamp.ShiftSubSeconds(-(micros() - receiveTimestamp));
			if (Encoder->DecodeInPacket(RawInPacket, InData, ReceiveTimestamp, ReceivingCounter, receivingDataSize))
			{
				if (ValidateCounter(ReceivingCounter))
				{
					// Counter accepted, update local tracker.
					LastValidReceivedCounter = ReceivingCounter;
				}
				else
				{
					// Event will trigger a report request, that should synchronize counters.
					OnEvent(PacketEventEnum::ReceiveRejectedCounter);
					return;
				}
			}
			else
			{
				OnEvent(PacketEventEnum::ReceiveRejectedMac);
				return;
			}
			break;
		default:
			return;
			break;
		}

		/// <summary>
		/// From here forward, the packet has been validated for:
		/// - Data integrity.
		/// - Source authenticity.
		/// - Replay/Echo denied.
		/// </summary>
		const uint8_t port = InData[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX];
		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			if (port == Unlinked::PORT)
			{
				OnUnlinkedPacketReceived(receiveTimestamp,
					&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
					LoLaPacketDefinition::GetPayloadSize(packetSize),
					ReceivingCounter);
			}
			break;
		case LinkStageEnum::Linking:
			if (port == Linking::PORT)
			{
				OnLinkingPacketReceived(receiveTimestamp,
					&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
					LoLaPacketDefinition::GetPayloadSize(packetSize),
					ReceivingCounter);
			}
			break;
		case LinkStageEnum::Linked:
			LastValidReceived = millis();

			NotifyPacketReceived(receiveTimestamp,
				InData[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX],
				LoLaPacketDefinition::GetPayloadSize(packetSize));
			break;
		default:
			return;
			break;
		}

		OnEvent(PacketEventEnum::Received);
	}

protected:
	/// <summary>
	/// Time since a valid packet was received.
	/// </summary>
	/// <returns>Elapsed time in milliseconds</returns>
	const uint32_t GetElapsedSinceLastValidReceived()
	{
		return millis() - LastValidReceived;
	}

	/// <summary>
	/// Restart the tracking, assume our last valid packet was just now.
	/// </summary>
	void ResetLastValidReceived()
	{
		LastValidReceived = millis();
	}

private:
	const bool ValidateCounter(const uint8_t counter)
	{
		const uint8_t counterRoll = counter - LastValidReceivedCounter;
		if (counterRoll > 0
			&& counterRoll < LoLaLinkDefinition::ROLLING_COUNTER_ERROR)
		{
			// Counter accepted, update local tracker.
			LastValidReceivedCounter = counter;

			if (counterRoll > 1)
			{
				OnReceiveLossDetected(counterRoll - 1);
			}

			return true;
		}
		else
		{
			return false;
		}
	}
};
#endif