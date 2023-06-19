// AbstractLoLaSender.h

#ifndef _ABSTRACT_LOLA_SENDER_
#define _ABSTRACT_LOLA_SENDER_

#include "AbstractLoLa.h"

template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaSender : public AbstractLoLa<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLa<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::Transceiver;
	using BaseClass::PacketService;
	using BaseClass::Encoder;
	using BaseClass::SyncClock;
	using BaseClass::LinkStage;

	using BaseClass::RawOutPacket;
	using BaseClass::ZeroTimestamp;

	using BaseClass::OnEvent;

private:
	Timestamp SendTimestamp{};

private:
	// Send duration estimation helpers.
	uint32_t SendShortDurationMicros = 0;
	uint32_t SendVariableDurationMicros = 0;

protected:
	// Rolling counter.
	uint8_t SendCounter = 0;


protected:
	virtual const uint8_t GetTxChannel(const uint32_t rollingMicros) { return 0; }

public:
	AbstractLoLaSender(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		IClockSource* clockSource,
		ITimerSource* timerSource)
		: BaseClass(scheduler, encoder, transceiver, clockSource, timerSource)
	{}

	virtual const bool SendPacket(const uint8_t* data, const uint8_t payloadSize) final
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			// Encode packet with no encryption and CRC.
			Encoder->EncodeOutPacket(data, RawOutPacket, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linking:
			// Encrypt packet without token.
			Encoder->EncodeOutPacket(data, RawOutPacket, ZeroTimestamp, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linked:
			// Encrypt packet with token based on time.
			Encoder->EncodeOutPacket(data,
				RawOutPacket,
				SendTimestamp,
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
			OnEvent(PacketEventEnum::Sent);

			return true;
		}

		return false;

	}

protected:
	const bool SetSendCalibration(const uint32_t shortDuration, const uint32_t longDuration)
	{
		if (shortDuration > 0 && longDuration > shortDuration)
		{
			SendShortDurationMicros = shortDuration;
			SendVariableDurationMicros = longDuration - shortDuration;
			return true;
		}

		return false;
	}

	const uint32_t GetSendDuration(const uint8_t payloadSize)
	{
		return SendShortDurationMicros
			+ ((SendVariableDurationMicros * payloadSize) / LoLaPacketDefinition::MAX_PAYLOAD_SIZE)
			+ Transceiver->GetTimeToAir(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

	const uint16_t GetOnAirDuration(const uint8_t payloadSize)
	{
		return Transceiver->GetDurationInAir(LoLaPacketDefinition::GetTotalSize(payloadSize));
	}

	const uint16_t GetHopDuration()
	{
		return Transceiver->GetTimeToHop();
	}

protected:
	/// <summary>
	/// Mock internal "SendPacket", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="data"></param>
	/// <param name="payloadSize"></param>
	/// <returns></returns>
	const bool MockSendPacket(const uint8_t* data, const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		// Encrypt packet with token based on time.
		Encoder->EncodeOutPacket(data, RawOutPacket, SendTimestamp, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));

		// Call Packet Service Send (mock) to include the call overhead.
		if (PacketService.MockSend(packetSize,
			GetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			return true;
		}

		return false;
	}
};
#endif