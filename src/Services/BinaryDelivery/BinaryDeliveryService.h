// BinaryDeliveryService.h

#ifndef _ABSTRACT_STREAM_h
#define _ABSTRACT_STREAM_h

#include "..\..\Services\AbstractLoLaDiscoveryService.h"
#include "..\..\LoLaLink\LoLaPacketDefinition.h"
#include <IBinaryDelivery.h>
#include "BinaryDeliveryDefinitions.h"

/// <summary>
/// Bi-directional binary delivery service.
/// Dual role class.
/// Does not perform discovery on link start, unless a request is issued in the meanwhile.
/// Piggybacks on discovery flow, with a dynamic Service Id, corresponding to the Binary to deliver.
/// Receiver must be set up and enabled, for sender to be able to proceed.
/// </summary>
template<const uint8_t Port>
class BinaryDeliveryService : public AbstractLoLaDiscoveryService<Port, false, BinaryDeliveryMaxPayloadSize, 5000>
	, public virtual IBinaryDeliveryService
{
private:
	static const uint32_t NoDiscoveryTimeOut = 5000;

	const uint8_t MaxChunkSize = 16;

	using BaseClass = AbstractLoLaDiscoveryService<Port, false, BinaryDeliveryMaxPayloadSize, NoDiscoveryTimeOut>;

	using BaseClass::OutPacket;

	using BaseClass::StopDiscovery;
	using BaseClass::StartDiscovery;
	using BaseClass::RequestSendPacket;

protected:
	enum DeliveryError
	{
		BinarySenderNotAvailable,
		BinaryReceiverNotAvailable,
		DiscoveryCollisionAfterReceive,
		DiscoveryCollisionAfterSend,
		DiscoveryWithDisabledService,
		ReceivedUnexpectedPacket,
	};

	virtual void OnDeliveryError(const DeliveryError error) {}

private:
	// Only one send request at a time, for each port/service.
	IBinarySender* RequestSender = nullptr;

	// Only one receive request at a time, for each port/service.
	IBinaryReceiver* RequestReceiver = nullptr;

	union ArrayToUint32 {
		uint8_t Array[sizeof(uint32_t)];
		uint32_t Value;
	} ATUI;

	enum DeliveryStateEnum
	{
		Disabled = 0,
		ReadyForReceive = 1,
		ReceiveComplete = 2,
		Sending = 3,
		SendingComplete = 4,
	} State = DeliveryStateEnum::Disabled;

	enum SendingStateEnum
	{
		SendingStart,
		SendingData,
		SendingEnd
	} SendingState = SendingStateEnum::SendingStart;
	uint32_t CurrentSendAddress = 0;

public:
	BinaryDeliveryService(Scheduler& scheduler, ILoLaLink* loLaLink, const uint32_t sendRequestTimeout = 100)
		: BaseClass(scheduler, loLaLink, sendRequestTimeout)
	{}

	virtual const bool Setup()
	{
		StopDiscovery();

		if (BaseClass::Setup())
		{
			return true;
		}
		return false;
	}

	virtual const bool RequestSendBinary(IBinarySender* sendRequest) final
	{
		RequestSender = sendRequest;

		if (RequestSender != nullptr)
		{
			State = DeliveryStateEnum::Sending;
			SendingState = SendingStateEnum::SendingStart;
			StartDiscovery();
			return true;
		}

		StopDiscovery();
		return false;
	}

	virtual const bool AttachReceiver(IBinaryReceiver* requestReceiver) final
	{
		RequestReceiver = requestReceiver;

		return RequestReceiver != nullptr;
	}

protected:
	virtual void OnDiscoveredService() final
	{
		switch (State)
		{
		case DeliveryStateEnum::ReadyForReceive:
			// Nothing to do, for now.
			Task::disable();
			break;
		case DeliveryStateEnum::ReceiveComplete:
			//TODO Validate CRC?
			RequestReceiver->OnFinishedReceiving();
			State = DeliveryStateEnum::Disabled;
			StopDiscovery();
			break;
		case DeliveryStateEnum::Sending:
			switch (SendingState)
			{
			case SendingStateEnum::SendingStart:
				OutPacket.SetPort(Port);

				OutPacket.Payload[SUB_HEADER_OFFSET] = SenderStartSubDefinition::SUB_HEADER;

				ATUI.Value = RequestSender->GetDataSize();
				OutPacket.Payload[SUB_PAYLOAD_OFFSET] = ATUI.Array[0];
				OutPacket.Payload[SUB_PAYLOAD_OFFSET + 1] = ATUI.Array[1];
				OutPacket.Payload[SUB_PAYLOAD_OFFSET + 2] = ATUI.Array[2];
				OutPacket.Payload[SUB_PAYLOAD_OFFSET + 3] = ATUI.Array[3];

				if (RequestSendPacket(SenderStartSubDefinition::PAYLOAD_SIZE))
				{
				}
				else
				{
					Task::delay(1);
				}

				break;
			case SendingStateEnum::SendingData:
				//TODO: check if ok.
				CurrentSendAddress += 1;//TODO: use variable offset.
				if (CurrentSendAddress > RequestSender->GetDataSize())
				{
					SendingState = SendingStateEnum::SendingEnd;
				}
				Task::enableIfNot();
				break;
			case SendingStateEnum::SendingEnd:
				//TODO: check if ok.
				State = DeliveryStateEnum::SendingComplete;
				Task::enableIfNot();
				break;
			default:
				break;
			}
			break;
		case DeliveryStateEnum::SendingComplete:
			break;
		case DeliveryStateEnum::Disabled:
		default:
			Task::disable();
			break;
		}
	}

	virtual void OnDiscoveredPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize)
	{
		switch (State)
		{
		case DeliveryStateEnum::Disabled:
			break;
		case DeliveryStateEnum::ReadyForReceive:
			//TODO: sub headers should only be stream start, data and end.

			return;
		case DeliveryStateEnum::ReceiveComplete:
			break;
		case DeliveryStateEnum::Sending:
			switch (SendingState)
			{
			case SendingStateEnum::SendingStart:
				if (payload[SUB_PAYLOAD_OFFSET])
					//TODO: check if ok.
					SendingState = SendingStateEnum::SendingData;
				CurrentSendAddress = 0;
				Task::enableIfNot();
				break;
			case SendingStateEnum::SendingData:
				//TODO: check if ok.
				CurrentSendAddress += 1;//TODO: use variable offset.
				if (CurrentSendAddress > RequestSender->GetDataSize())
				{
					SendingState = SendingStateEnum::SendingEnd;
				}
				Task::enableIfNot();
				break;
			case SendingStateEnum::SendingEnd:
				//TODO: check if ok.
				State = DeliveryStateEnum::SendingComplete;
				Task::enableIfNot();
				break;
			default:
				break;
			}
			return;
		case DeliveryStateEnum::SendingComplete:
			break;
		default:
			break;
		}

		OnDeliveryError(DeliveryError::ReceivedUnexpectedPacket);
	}


	virtual const uint8_t GetServiceId() final
	{
		switch (State)
		{
		case DeliveryStateEnum::ReadyForReceive:
			if (RequestReceiver != nullptr)
			{
				return RequestReceiver->GetId();
			}
			else
			{
				OnDeliveryError(DeliveryError::BinaryReceiverNotAvailable);
			}
		case DeliveryStateEnum::ReceiveComplete:
			OnDeliveryError(DeliveryError::DiscoveryCollisionAfterReceive);
			return 0;
			break;
		case DeliveryStateEnum::Sending:
			if (RequestSender != nullptr)
			{
				return RequestSender->GetId();
			}
			else
			{
				OnDeliveryError(DeliveryError::BinarySenderNotAvailable);
				return 0;
			}
			break;
		case DeliveryStateEnum::SendingComplete:
			OnDeliveryError(DeliveryError::DiscoveryCollisionAfterSend);
			return 0;
		case DeliveryStateEnum::Disabled:
			OnDeliveryError(DeliveryError::DiscoveryWithDisabledService);
		default:
			return 0;
		}
	}
};
#endif