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
	using BaseClass::PacketService;
	using BaseClass::Driver;
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

	/// <summary>
	/// Encode data to RawOutPacket.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	virtual void EncodeOutPacket(const uint8_t* data, const uint8_t counter, const uint8_t dataSize) {}

	/// <summary>
	/// Encode data to RawOutPacket.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="timestamp"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	virtual void EncodeOutPacket(const uint8_t* data, Timestamp& timestamp, const uint8_t counter, const uint8_t dataSize) { }


public:
	AbstractLoLaSender(Scheduler& scheduler, ILoLaPacketDriver* driver, IClockSource* clockSource, ITimerSource* timerSource)
		: BaseClass(scheduler, driver, clockSource, timerSource)
	{}

	virtual const bool SendPacket(ILinkPacketSender* callback, const uint8_t* data, const uint8_t payloadSize) final
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			// Encode packet with no encryption and CRC.
			EncodeOutPacket(data, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linking:
			// Encrypt packet without token.
			EncodeOutPacket(data, ZeroTimestamp, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		case LinkStageEnum::Linked:
			// Encrypt packet with token based on time.
			EncodeOutPacket(data,
				SendTimestamp,
				SendCounter,
				LoLaPacketDefinition::GetDataSize(packetSize));
			break;
		default:
			break;
		}

		if (PacketService.Send(callback,
			packetSize,
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
			+ Driver->GetTransmitDurationMicros(LoLaPacketDefinition::GetTotalSize(payloadSize));
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
	const bool MockSendPacket(ILinkPacketSender* callback, const uint8_t* data, const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		// Encrypt packet with token based on time.
		EncodeOutPacket(data, SendTimestamp, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
		/*EncodeOutPacket(data,
			SendTimestamp.Seconds,
			SendTimestamp.GetRollingMicros() / LoLaLinkDefinition::SUB_TOKEN_PERIOD_MICROS,
			SendCounter,
			LoLaPacketDefinition::GetDataSize(packetSize));*/

		// Call Packet Service Send (mock) to include the call overhead.
		if (PacketService.MockSend(
			callback,
			packetSize,
			GetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			return true;
		}

		return false;
	}
};
#endif