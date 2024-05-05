// AbstractDeliveryService.h

#ifndef _ABSTRACT_DELIVERY_SERVICE_
#define _ABSTRACT_DELIVERY_SERVICE_

#include "../../Services/Discovery/AbstractDiscoveryService.h"
#include "DeliveryDefinitions.h"

/// <summary>
/// Bi-directional, assured delivery service.
/// Extends DiscoveryService and its functionality.
/// Abstracts packet handling as delivery of as data.
/// </summary>
template<const uint8_t Port,
	const uint32_t ServiceId>
class AbstractDeliveryService
	: public AbstractDiscoveryService<Port, ServiceId, LoLaPacketDefinition::MAX_PAYLOAD_SIZE>
{
private:
	static constexpr uint32_t DELIVERY_TIMEOUT_MILLIS = 1000;
	static constexpr uint32_t ACK_TIMEOUT_MILLIS = 5000;

private:
	using BaseClass = AbstractDiscoveryService<Port, ServiceId, LoLaPacketDefinition::MAX_PAYLOAD_SIZE>;

	enum class TxStateEnum : uint8_t
	{
		None,
		WaitingForReply,
		AcknowledgingReply
	};

	enum class RxStateEnum
	{
		None,
		ReplyPending
	};

private:
	using BaseClass::CanRequestSend;
	using BaseClass::OutPacket;
	using BaseClass::RequestSendPacket;
	using BaseClass::PacketThrottle;
	using BaseClass::ResetPacketThrottle;

protected:
	using BaseClass::HasLink;
	using BaseClass::IsDiscovered;

private:
	const uint8_t* TxData = nullptr;

	uint32_t TxStart = 0;
	uint32_t RxStart = 0;
	TxStateEnum TxState = TxStateEnum::None;
	RxStateEnum RxState = RxStateEnum::None;
	uint8_t TxId = 0;
	uint8_t RxId = 0;
	uint8_t TxDataSize = 0;
	uint8_t TxPriority = 0;

protected:
	virtual void OnDeliveryComplete() {}

	virtual void OnDeliveryReceived(const uint32_t timestamp, const uint8_t* data, const uint8_t dataSize) {}

public:
	AbstractDeliveryService(Scheduler& scheduler, ILoLaLink* loLaLink)
		: BaseClass(scheduler, loLaLink)
	{}

protected:
	void CancelDelivery()
	{
		TxState = TxStateEnum::None;
		Task::enableDelayed(0);
	}

	const bool DeliveryTaskPending() const
	{
		return TxState != TxStateEnum::None || RxState != RxStateEnum::None;
	}

	const bool CanRequestDelivery()
	{
		return HasLink()
			&& IsDiscovered()
			&& CanRequestSend()
			&& !DeliveryTaskPending();
	}

	const bool RequestDelivery(const uint8_t* data, const uint8_t dataSize, const RequestPriority priority = RequestPriority::REGULAR)
	{
		return RequestDelivery(data, dataSize, (uint8_t)priority);
	}
	/// <summary>
	/// 
	/// </summary>
	/// <param name="data">Immutable data source. Contents should not change before OnDeliveryComplete.</param>
	/// <param name="dataSize">[0 ; DeliveryDefinitions::MAX_DATA_SIZE]. Should not change before OnDeliveryComplete.</param>
	/// <returns>True on success.</returns>
	const bool RequestDelivery(const uint8_t* data, const uint8_t dataSize, const uint8_t priority)
	{
		if (HasLink()
			&& IsDiscovered()
			&& CanRequestSend()
			&& data != nullptr
			&& dataSize <= DeliveryDefinitions::MAX_DATA_SIZE)
		{
			TxState = TxStateEnum::WaitingForReply;
			TxId++;

			//Data = (uint8_t*)data;
			TxData = data;
			TxDataSize = dataSize;
			TxPriority = priority;

			Task::enableDelayed(0);

			ResetPacketThrottle();
			TxStart = millis();

			return true;
		}

		return false;
	}

protected:
	virtual void OnLinkedServiceRun()
	{
		switch (TxState)
		{
		case TxStateEnum::WaitingForReply:
			// Keep sending the data until we get a reply.
			if (CanRequestSend() && PacketThrottle())
			{
				OutPacket.SetPort(Port);
				OutPacket.SetHeader(DeliveryDefinitions::DeliveryRequestDefinition::HEADER);
				OutPacket.Payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_REQUEST_ID_INDEX] = TxId;
				memcpy(&OutPacket.Payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_DATA_INDEX],
					TxData, TxDataSize);

				RequestSendPacket(DeliveryDefinitions::GetDeliveryPayloadSize(TxDataSize), TxPriority);
			}
			else if (millis() - TxStart > DELIVERY_TIMEOUT_MILLIS)
			{
				TxId += INT8_MAX;
				TxStart = millis();
#if defined(DEBUG_LOLA)
				Serial.println(F("Delivery Id time out."));
#endif
			}
			Task::enableDelayed(0);
			break;
		case TxStateEnum::AcknowledgingReply:
			// Send Ack one time.
			if (CanRequestSend())
			{
				OutPacket.SetPort(Port);
				OutPacket.SetHeader(DeliveryDefinitions::DeliveryAckDefinition::HEADER);
				OutPacket.Payload[DeliveryDefinitions::DeliveryAckDefinition::PAYLOAD_REQUEST_ID_INDEX] = TxId;

				if (RequestSendPacket(DeliveryDefinitions::DeliveryAckDefinition::PAYLOAD_SIZE, RequestPriority::FAST))
				{
					TxState = TxStateEnum::None;
				}
			}
			Task::enableDelayed(0);
			break;
		case TxStateEnum::None:
		default:
			break;
		}

		switch (RxState)
		{
		case RxStateEnum::ReplyPending:
			// Keep sending the reply until we get an ack or the service starts another delivery request.
			if (CanRequestSend()
				&& PacketThrottle())
			{
				OutPacket.SetPort(Port);
				OutPacket.SetHeader(DeliveryDefinitions::DeliveryReplyDefinition::HEADER);
				OutPacket.Payload[DeliveryDefinitions::DeliveryReplyDefinition::PAYLOAD_REQUEST_ID_INDEX] = RxId;
				RequestSendPacket(DeliveryDefinitions::DeliveryReplyDefinition::PAYLOAD_SIZE, RequestPriority::FAST);
			}
			else if (millis() - RxStart > ACK_TIMEOUT_MILLIS)
			{
				RxState = RxStateEnum::None;
#if defined(DEBUG_LOLA)
				Serial.println(F("Delivery Reply timed out."));
#endif
			}
			break;
		case RxStateEnum::None:
		default:
			break;
		}
	}

	virtual void OnServiceStarted()
	{
		CancelDelivery();

		Task::disable();
		RxId = 0;
		TxId = 0;
		TxState = TxStateEnum::None;
		RxState = RxStateEnum::None;
	}

	void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case DeliveryDefinitions::DeliveryRequestDefinition::HEADER:
			switch (RxState)
			{
			case RxStateEnum::None:
				RxState = RxStateEnum::ReplyPending;
				break;
			case RxStateEnum::ReplyPending:
				if (RxId != payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_REQUEST_ID_INDEX])
				{
					// Interrupted Ack, following up a request
					//  before Ack completes will invalidate the previous request.
#if defined(DEBUG_LOLA)
					Serial.println(F("Delivery Reply override."));
#endif
				}
			default:
				break;
			}

			if (RxId != payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_REQUEST_ID_INDEX])
			{
				RxId = payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_REQUEST_ID_INDEX];
				OnDeliveryReceived(timestamp,
					&payload[DeliveryDefinitions::DeliveryRequestDefinition::PAYLOAD_DATA_INDEX],
					DeliveryDefinitions::GetDeliveryDataSize(payloadSize));
				Task::enableDelayed(0);
				RxStart = millis();
			}
			break;
		case DeliveryDefinitions::DeliveryReplyDefinition::HEADER:
			switch (TxState)
			{
			case TxStateEnum::WaitingForReply:
				Task::enableDelayed(0);
				if (payload[DeliveryDefinitions::DeliveryReplyDefinition::PAYLOAD_REQUEST_ID_INDEX] == TxId)
				{
					// Expected reply after a delivery request.
					TxState = TxStateEnum::AcknowledgingReply;
					OnDeliveryComplete();
				}
				else
				{
					// Invalid reply, shuffle id to start again.
					TxId += INT8_MAX;
					TxStart = millis();

					ResetPacketThrottle();
#if defined(DEBUG_LOLA)
					Serial.println(F("Delivery Id mismatch."));
#endif
				}
				break;
			case TxStateEnum::None:
				if (payload[DeliveryDefinitions::DeliveryReplyDefinition::PAYLOAD_REQUEST_ID_INDEX] == TxId)
				{
					// Sender thought it was free, but the  receiver
					//  didn't get the Ack or sender has moved on to another request.
					Task::enableDelayed(0);
					TxState = TxStateEnum::AcknowledgingReply;
				}
#if defined(DEBUG_LOLA)
				else
				{
					Serial.println(F("Delivery Unexpected reply."));
				}
#endif
				break;
			case TxStateEnum::AcknowledgingReply:
				// Sender was already trying to send an Ack.
				Task::enableDelayed(0);
				break;
			default:
				break;
			}
			break;
		case DeliveryDefinitions::DeliveryAckDefinition::HEADER:
			if (RxState == RxStateEnum::ReplyPending
				&& payload[DeliveryDefinitions::DeliveryAckDefinition::PAYLOAD_REQUEST_ID_INDEX] == RxId)
			{
				RxState = RxStateEnum::None;
				Task::enableDelayed(0);
			}
			break;
		default:
			break;
		}
	}
};
#endif