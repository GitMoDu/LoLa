// AbstractLoLaSender.h

#ifndef _ABSTRACT_LOLA_SENDER_
#define _ABSTRACT_LOLA_SENDER_

#include "AbstractLoLa.h"

class AbstractLoLaSender : public AbstractLoLa
{
private:
	using BaseClass = AbstractLoLa;

private:
	Timestamp SendTimestamp{};

private:
	// Send duration estimation helpers.
	uint_fast16_t SendShortDurationMicros = 0;
	uint_fast16_t SendVariableDurationMicros = 0;

protected:
	// Rolling counter.
	uint8_t SendCounter = 0;


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

	virtual const bool SendPacket(const uint8_t* data, const uint8_t payloadSize) final
	{
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			// Encode packet with no encryption and CRC.
			Encoder->EncodeOutPacket(data, RawOutPacket, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linking:
			// Encrypt packet without token.
			Encoder->EncodeOutPacket(data, RawOutPacket, 0, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linked:
			// Encrypt packet with token based on time.
			Encoder->EncodeOutPacket(data,
				RawOutPacket,
				SendTimestamp.Seconds,
				SendCounter,
				LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		default:
			break;
		}

		if (PacketService.Send(packetSize,
			GetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			SendCounter++;

			return true;
		}

		return false;

	}

protected:
	const bool SetSendCalibration(const uint32_t shortDuration, const uint32_t longDuration)
	{
		const uint32_t fullSendDuration = SendShortDurationMicros
			+ ((SendVariableDurationMicros * LoLaPacketDefinition::MAX_PAYLOAD_SIZE) / LoLaPacketDefinition::MAX_PAYLOAD_SIZE)
			+ Transceiver->GetTimeToAir(LoLaPacketDefinition::GetTotalSize(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));

		if (shortDuration > 0
			&& longDuration >= shortDuration
			&& shortDuration < UINT16_MAX
			&& longDuration < UINT16_MAX
			&& fullSendDuration < UINT16_MAX)
		{
			SendShortDurationMicros = shortDuration;
			SendVariableDurationMicros = longDuration - shortDuration;

#if defined(DEBUG_LOLA)
			Serial.println(F("Time-to-Air estimation"));
			Serial.println(F("Short\tLong"));
			Serial.print(GetSendDuration(0));
			Serial.print('\t');
			Serial.print(GetSendDuration(LoLaPacketDefinition::MAX_PAYLOAD_SIZE));
			Serial.println(F(" us"));
#endif
			return true;
		}

		return false;
	}

	const uint16_t GetSendDuration(const uint8_t payloadSize)
	{
		return SendShortDurationMicros
			+ ((((uint32_t)SendVariableDurationMicros) * payloadSize) / LoLaPacketDefinition::MAX_PAYLOAD_SIZE)
			+ Transceiver->GetTimeToAir(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

	const uint16_t GetOnAirDuration(const uint8_t payloadSize)
	{
		return Transceiver->GetDurationInAir(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

protected:
	/// <summary>
	/// Mock internal "SendPacket", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="payloadSize"></param>
	/// <returns></returns>
	const bool MockSendPacket(const uint8_t* data, const uint8_t payloadSize)
	{
		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		// Encrypt packet with token based on time.
		Encoder->EncodeOutPacket(data, RawOutPacket, SendTimestamp.Seconds, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));

		// Call Packet Service Send (mock) to include the call overhead.
		if (PacketService.MockSend(packetSize,
			MockGetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			return true;
		}

		return false;
	}
};
#endif