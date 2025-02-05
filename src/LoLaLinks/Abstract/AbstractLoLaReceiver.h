// AbstractLoLaReceiver.h

#ifndef _ABSTRACT_LOLA_RECEIVER_
#define _ABSTRACT_LOLA_RECEIVER_

#include "AbstractLoLaSender.h"

class AbstractLoLaReceiver : public AbstractLoLaSender
{
private:
	using BaseClass = AbstractLoLaSender;

private:
	// The incoming plaintext content is decrypted to here, from the RawInPacket.
	uint8_t InData[LoLaPacketDefinition::GetDataSize(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)]{};

private:
	Timestamp RxTimestamp{};

private:
	/// <summary>
	/// Rolling packet counter.
	/// </summary>
	uint16_t ReceiveCounter = 0;

protected:
	uint16_t ReceivedCounter = 0;

protected:
	/// <summary>
	/// Packet parser for Link implementation classes. Only fires when link is not active.
	///// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="rollingCounter"></param>
	/// <param name="payloadSize"></param>
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize) {}

	/// <summary>
	/// Packet parser for Link implementation classes. Only fires when linking is active.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	virtual void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) {}

	/// <summary>
	/// Inform any Link class of RSSI quality and packet loss.
	/// </summary>
	/// <param name="rssi"></param>
	/// <param name="lostCount"></param>
	virtual void OnPacketReceivedOk(const uint8_t rssi, const uint16_t lostCount) {}

public:
	AbstractLoLaReceiver(TS::Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		ILoLaTransceiver* transceiver,
		ICycles* cycles)
		: BaseClass(scheduler, linkRegistry, transceiver, cycles)
	{}

public:
	/// <summary>
	/// RawInPacket packet is validated for:
	/// - Data integrity.
	/// - Source authenticity.
	/// - Replay/Echo denied.
	/// And decoded into InData.
	/// <param name="receiveTimestamp">micros() timestamp of packet start.</param>
	/// <param name="packetSize"></param>
	/// <param name="rssi"></param>
	void OnReceived(const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		const uint8_t receivingDataSize = LoLaPacketDefinition::GetDataSize(packetSize);

		uint16_t receivingCounter = 0;
		uint16_t receivingLost = 0;

		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Sleeping:
		case LinkStageEnum::Searching:
		case LinkStageEnum::Pairing:
		case LinkStageEnum::SwitchingToLinking:
			// Update MAC without implicit addressing or token.
			// Addressing must be explicit in payload.
			if (Session.DecodeInPacket(RawInPacket, InData, receivingCounter, receivingDataSize))
			{
				// Check for valid port.
				if (InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Port - (uint8_t)LoLaPacketDefinition::IndexEnum::Data] == LoLaLinkDefinition::LINK_PORT)
				{
					OnUnlinkedPacketReceived(receiveTimestamp,
						&InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Payload - (uint8_t)LoLaPacketDefinition::IndexEnum::Data],
						receivingCounter,
						LoLaPacketDefinition::GetPayloadSize(packetSize));
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
		case LinkStageEnum::Authenticating:
		case LinkStageEnum::ClockSyncing:
		case LinkStageEnum::SwitchingToLinked:
			// Update MAC with implicit addressing but without token.
			if (Session.DecodeInPacket(RawInPacket, InData, 0, receivingCounter, receivingDataSize))
			{
				// Validate counter and check for valid port.
				if (InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Port - (uint8_t)LoLaPacketDefinition::IndexEnum::Data] == LoLaLinkDefinition::LINK_PORT
					&& ValidateCounter(receivingCounter, receivingLost))
				{
					OnLinkingPacketReceived(receiveTimestamp,
						&InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Payload - (uint8_t)LoLaPacketDefinition::IndexEnum::Data],
						LoLaPacketDefinition::GetPayloadSize(packetSize));

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
			LOLA_RTOS_PAUSE();
			SyncClock.GetTimestamp(RxTimestamp);
			RxTimestamp.ShiftSubSeconds(-((int32_t)(micros() - receiveTimestamp)));
			LOLA_RTOS_RESUME();
			if (Session.DecodeInPacket(RawInPacket, InData, RxTimestamp.Seconds, receivingCounter, receivingDataSize))
			{
				// Validate counter and check for valid port.
				if (ValidateCounter(receivingCounter, receivingLost))
				{
					Registry->NotifyPacketListener(receiveTimestamp,
						&InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Payload - (uint8_t)LoLaPacketDefinition::IndexEnum::Data],
						LoLaPacketDefinition::GetPayloadSize(packetSize),
						InData[(uint8_t)LoLaPacketDefinition::IndexEnum::Port - (uint8_t)LoLaPacketDefinition::IndexEnum::Data]);

					ReceivedCounter++;
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
	void SetReceiveCounter(const uint16_t counter)
	{
		ReceiveCounter = counter;
	}

	const bool MockReceiveFailPacket(const uint32_t receiveTimestamp, const uint8_t* data, const uint8_t payloadSize)
	{
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		uint16_t receivingCounter = 0;

		SyncClock.GetTimestamp(RxTimestamp);
		RxTimestamp.ShiftSubSeconds(-((int32_t)(micros() - receiveTimestamp)));

		// (Fail to) Decrypt packet with token based on time.
		return Session.DecodeInPacket(data, RawInPacket, RxTimestamp.Seconds, receivingCounter, LoLaPacketDefinition::GetDataSize(packetSize));
	}

private:
	/// <summary>
	/// Link will tolerate some dropped packets by forwarding the rolling counter.
	/// </summary>
	/// <param name="counter">Received counter.</param>
	/// <param name="receiveLost">Resulting counter roll. [1 ; INT16_MAX] for typical operation, 0 when invalid.</param>
	/// <returns>True on valid counter.</returns>
	const bool ValidateCounter(const uint16_t counter, uint16_t& receiveLost)
	{
		const uint16_t counterRoll = counter - ReceiveCounter;
		if (counterRoll < (uint16_t)INT16_MAX
			&& counterRoll > 0
			)
		{
			// Counter accepted, update local tracker.
			ReceiveCounter = counter;
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