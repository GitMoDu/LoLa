// AbstractLoLaSender.h

#ifndef _ABSTRACT_LOLA_SENDER_
#define _ABSTRACT_LOLA_SENDER_

#include "AbstractLoLaLinkPacket.h"

template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaSender : public AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::Driver;
	using BaseClass::SyncClock;
	using BaseClass::LinkStage;
	using BaseClass::PacketService;

	using BaseClass::OutPacket;

	using BaseClass::RawOutPacket;
	using BaseClass::ZeroTimestamp;

	using BaseClass::OnEvent;
	using BaseClass::GetTxChannel;
	using BaseClass::GetSendDuration;

private:
	const uint32_t AckSendDuration = GetSendDuration(AckDefinition::PAYLOAD_SIZE);


	struct AckRequestStruct
	{
		uint32_t Timeout = 0;
		uint8_t Port = 0;
		uint8_t Id = 0;
		bool Pending = false;
	};

	struct AckSendStruct
	{
		uint8_t Port = 0;
		uint8_t Id = 0;
		uint8_t Handle = 0;

		void Clear()
		{
			Port = AckDefinition::PORT;
		}

		void Tag(const uint8_t handle, const uint8_t port, const uint8_t id)
		{
			Handle = handle;
			Port = port;
			Id = id;
		}

		const bool HasPending()
		{
			return Port != AckDefinition::PORT;
		}
	};


	TemplateLoLaOutDataPacket<AckDefinition::PAYLOAD_SIZE> AckPacket;
	AckRequestStruct AckRequest;

	Timestamp SendTimestamp;


protected:
	AckSendStruct AckSend;

	// Rolling counter.
	uint8_t SendCounter = 0;


protected:
	virtual void OnAckSent(const uint8_t port, const uint8_t id) {	}

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
	AbstractLoLaSender(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, driver, entropySource, clockSource, timerSource, duplex, hop)
		, AckPacket(AckDefinition::PORT)
		, AckRequest()
		, SendTimestamp()
		, AckSend()
	{}

	virtual const bool SendPacket(ILinkPacketSender* callback, uint8_t& handle, const uint8_t* data, const uint8_t payloadSize) final
	{
		return TemplateSendPacket<false>(callback, handle, data, payloadSize);
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			break;
		case LinkStageEnum::AwaitingLink:
			SendCounter = 0;
			break;
			//case LinkStageEnum::TransitionToLinking:
				//break;
		case LinkStageEnum::Linking:
			break;
			//case LinkStageEnum::TransitionToLinked:
				//break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}

protected:
	const bool SendPacketWithAck(ILinkPacketSender* callback, uint8_t& handle, const uint8_t* data, const uint8_t payloadSize)
	{
		return TemplateSendPacket<true>(callback, handle, data, payloadSize);
	}

	void RequestSendAck(const uint8_t port, const uint8_t id)
	{
		// Clear any pending send request, Ack takes priority.
		// PacketService will notify any previous sender of Send failure.
		PacketService.ClearSendRequest();

		AckRequest.Port = port;
		AckRequest.Id = id;

		AckRequest.Timeout = micros() + LoLaLinkDefinition::TRANSMIT_BASE_TIMEOUT_MICROS + GetSendDuration(AckDefinition::PAYLOAD_SIZE);
		AckRequest.Pending = true;
	}

	const bool CheckPendingAck()
	{
		if (AckRequest.Pending)
		{
			if (PacketService.CanSendPacket() && SendAckPacket())
			{
				AckRequest.Pending = false;
				OnAckSent(AckRequest.Port, AckRequest.Id);
				OnEvent(PacketEventEnum::SentAck);
			}
			else if (micros() > AckRequest.Timeout)
			{
				AckRequest.Pending = false;
				OnEvent(PacketEventEnum::SendAckFailed);
			}

			Task::enable();
			return true;
		}

		return false;
	}

	/// <summary>
	/// Mock internal "SendPacket", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="handle"></param>
	/// <param name="data"></param>
	/// <param name="payloadSize"></param>
	/// <returns></returns>
	const bool MockSendPacket(ILinkPacketSender* callback, uint8_t& handle, const uint8_t* data, const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);
		// Set rolling counter Id.
		handle = SendCounter;

		// Encrypt packet with token based on time.
		EncodeOutPacket(data, SendTimestamp, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
		/*EncodeOutPacket(data,
			SendTimestamp.Seconds,
			SendTimestamp.GetRollingMicros() / LoLaLinkDefinition::SUB_TOKEN_PERIOD_MICROS,
			SendCounter,
			LoLaPacketDefinition::GetDataSize(packetSize));*/

			// Call Packet Service Send (mock) to include the call overhead.
		if (PacketService.MockSendPacket(
			callback,
			handle,
			packetSize,
			GetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			return true;
		}

		return false;
	}

private:
	const bool SendAckPacket()
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(AckDefinition::PAYLOAD_SIZE));

		// Ack Port permanently set in constructor.
		// Quickly set payload of: the asking port and its rolling counter id.
		AckPacket.Payload[AckDefinition::PORT_OFFSET] = AckRequest.Port;
		AckPacket.Payload[AckDefinition::ID_OFFSET] = AckRequest.Id;

		// Set MAC/CRC and handle link mode.
		switch (LinkStage)
		{
			//case LinkStageEnum::TransitionToLinking:
		case LinkStageEnum::AwaitingLink:
			EncodeOutPacket(AckPacket.Data, SendCounter, AckDefinition::DATA_SIZE);
			break;
			//case LinkStageEnum::TransitionToLinked:
		case LinkStageEnum::Linking:
			EncodeOutPacket(AckPacket.Data, ZeroTimestamp, SendCounter, AckDefinition::DATA_SIZE);
			break;
		case LinkStageEnum::Linked:
		default:
			OnEvent(PacketEventEnum::SendAckFailed);
			// No ACKs during Linked.
			return false;
		}

		// Transmit without callback.
		if (PacketService.SendPacket(
			nullptr,
			SendCounter,
			AckDefinition::PACKET_SIZE,
			GetTxChannel(0)))
		{
			SendCounter++;

			return true;
		}
		else
		{
			// Driver refused to TX, most likely to due to concurring RX.
			OnEvent(PacketEventEnum::SendCollisionFailed);

			return false;
		}
	}

	template<const bool HasAck>
	const bool TemplateSendPacket(ILinkPacketSender* callback, uint8_t& handle, const uint8_t* data, const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(SendTimestamp);
		SendTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint8_t packetSize = LoLaPacketDefinition::GetTotalSize(payloadSize);

		// Set rolling counter Id.
		handle = SendCounter;

		switch (LinkStage)
		{
			//case LinkStageEnum::TransitionToLinking:
		case LinkStageEnum::AwaitingLink:
			// Encode packet with no encryption and CRC.
			EncodeOutPacket(data, SendCounter, LoLaPacketDefinition::GetDataSize(packetSize));
			break;
			//case LinkStageEnum::TransitionToLinked:
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

		if (HasAck)
		{
			if (PacketService.SendPacketWithAck(callback,
				handle,
				RawOutPacket,
				packetSize,
				GetTxChannel(SendTimestamp.GetRollingMicros())))
			{
				AckSend.Tag(handle, data[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX], SendCounter);
				SendCounter++;
				OnEvent(PacketEventEnum::SentWithAck);

				return true;
			}
		}
		else if (PacketService.SendPacket(callback,
			handle,
			packetSize,
			GetTxChannel(SendTimestamp.GetRollingMicros())))
		{
			SendCounter++;
			OnEvent(PacketEventEnum::Sent);

			return true;
		}

		return false;
	}
};
#endif