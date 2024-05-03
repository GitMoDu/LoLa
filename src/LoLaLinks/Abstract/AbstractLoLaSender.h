// AbstractLoLaSender.h

#ifndef _ABSTRACT_LOLA_SENDER_
#define _ABSTRACT_LOLA_SENDER_

#include "AbstractLoLa.h"

class AbstractLoLaSender : public AbstractLoLa
{
private:
	using BaseClass = AbstractLoLa;

private:
	Timestamp TxTimestamp{};
	uint32_t SentTimestamp = 0;

private:
	// Send duration estimation helpers.
	uint16_t SendShortDurationMicros = 0;
	uint16_t SendVariableDurationMicros = 0;

	// Rolling counter.
	uint16_t SendCounter = 0;

protected:
	uint16_t SentCounter = 0;

protected:
	virtual const uint8_t GetTxChannel(const uint32_t rollingMicros) { return 0; }
	virtual const uint8_t MockGetTxChannel(const uint32_t rollingMicros) { return 0; }

public:
	AbstractLoLaSender(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles)
		: BaseClass(scheduler, linkRegistry, encoder, transceiver, cycles)
	{}

	virtual const uint32_t GetSendElapsed() final
	{
		return micros() - SentTimestamp;
	}

	virtual const bool SendPacket(const uint8_t* data, const uint8_t payloadSize) final
	{
		SyncClock.GetTimestamp(TxTimestamp);
		TxTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t dataSize = LoLaPacketDefinition::GetDataSizeFromPayloadSize(payloadSize);
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			// Encode packet with no encryption.
			Encoder->EncodeOutPacket(data, RawOutPacket, SendCounter, dataSize);
			break;
		case LinkStageEnum::Linking:
			// Encrypt packet without token.
			Encoder->EncodeOutPacket(data, RawOutPacket,
				0,
				SendCounter, dataSize);
			break;
		case LinkStageEnum::Linked:
			// Encrypt packet with token based on time.
			Encoder->EncodeOutPacket(data, RawOutPacket,
				TxTimestamp.Seconds,
				SendCounter, dataSize);
			break;
		default:
			break;
		}

		if (PacketService.Send(packetSize,
			GetTxChannel(TxTimestamp.GetRollingMicros())))
		{
			SentTimestamp = micros();
			SendCounter++;
			SentCounter++;

			return true;
		}

		return false;
	}

protected:
	const bool SetSendCalibration(const uint32_t shortDuration, const uint32_t longDuration)
	{
		const uint16_t airShort = Transceiver->GetTimeToAir(LoLaPacketDefinition::GetTotalSize(0));
		const uint16_t airLong = Transceiver->GetTimeToAir(LoLaPacketDefinition::GetTotalSize(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));

		const uint32_t longestSend = longDuration + airLong;

		if (shortDuration > 0
			&& longDuration >= shortDuration
			&& shortDuration < UINT16_MAX
			&& longDuration < UINT16_MAX
			&& longestSend < UINT16_MAX)
		{
			SendShortDurationMicros = shortDuration + airShort;
			SendVariableDurationMicros = (longDuration - shortDuration) + (airLong - airShort);

			return true;
		}

		return false;
	}

	const uint16_t GetSendDuration(const uint8_t payloadSize)
	{
		return SendShortDurationMicros
			+ (uint16_t)((((uint32_t)SendVariableDurationMicros) * payloadSize) / LoLaPacketDefinition::MAX_PAYLOAD_SIZE);
	}

	const uint16_t GetOnAirDuration(const uint8_t payloadSize)
	{
		return Transceiver->GetDurationInAir(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

protected:
	void SetSendCounter(const uint16_t counter)
	{
		SendCounter = counter;
	}

	/// <summary>
	/// Mock internal "SendPacket", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="payloadSize"></param>
	/// <returns></returns>
	const bool MockSendPacket(const uint8_t* data, const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(TxTimestamp);
		TxTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t dataSize = LoLaPacketDefinition::GetDataSizeFromPayloadSize(payloadSize);
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		// Encrypt packet with token based on time.
		Encoder->EncodeOutPacket(data, RawOutPacket, TxTimestamp.Seconds, SendCounter, dataSize);

		// Call Packet Service Send (mock) to include the call overhead.
		if (PacketService.MockSend(packetSize,
			MockGetTxChannel(TxTimestamp.GetRollingMicros())))
		{
			return true;
		}

		return false;
	}
};
#endif