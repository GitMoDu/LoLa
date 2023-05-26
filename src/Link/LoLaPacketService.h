// LoLaPacketService.h

#ifndef _LOLA_PACKET_DRIVER_h
#define _LOLA_PACKET_DRIVER_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>
#include "IPacketServiceListener.h"


/// <summary>
/// Packet send and receive service, with a single IPacketServiceListener for receive.
/// Interfaces directly with and abstracts ILoLaTransceiver.
/// Optional SendListener event callbacks.
/// Properties:
///		Content abstract.
///		Buffers input packets, so transceiver is free to receive more while the service processes it.
/// </summary>
/// <param name="MaxPacketSize">The maximum raw packet size.</param>
template<const uint8_t MaxPacketSize>
class LoLaPacketService : private Task, public virtual ILoLaTransceiverListener
{
private:
	using SendResultEnum = ILinkPacketSender::SendResultEnum;

	enum StateEnum
	{
		Done,
		Sending,
		SendingSuccess,
		SendingError
	};

	static constexpr uint32_t SEND_CHECK_PERIOD_MILLIS = 1;

private:
	IPacketServiceListener* ServiceListener;

	uint8_t* RawInPacket = nullptr;
	uint8_t* RawOutPacket = nullptr;

	ILinkPacketSender* SendListener = nullptr;

	uint32_t SendOutTimestamp = 0;

	volatile uint32_t ReceiveTimestamp = 0;
	volatile uint8_t PendingReceiveSize = 0;
	volatile uint8_t PendingReceiveRssi = 0;

	volatile StateEnum State = StateEnum::Done;

public:
	ILoLaTransceiver* Transceiver = nullptr;

public:
	LoLaPacketService(Scheduler& scheduler,
		IPacketServiceListener* serviceListener,
		ILoLaTransceiver* transceiver,
		uint8_t* rawInPacket,
		uint8_t* rawOutPacket)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILoLaTransceiverListener()
		, ServiceListener(serviceListener)
		, RawInPacket(rawInPacket)
		, RawOutPacket(rawOutPacket)
		, Transceiver(transceiver)
	{}

	const bool Setup()
	{
		if (RawInPacket != nullptr &&
			RawOutPacket != nullptr &&
			Transceiver->SetupListener(this))
		{
			return true;
		}

		return false;
	}

	virtual bool Callback() final
	{
		switch (State)
		{
		case StateEnum::SendingSuccess:
			State = StateEnum::Done;
			Task::enable();
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::Success);
				SendListener = nullptr;
			}
			break;
		case StateEnum::Sending:
			if (micros() - SendOutTimestamp > LoLaLinkDefinition::TRANSMIT_BASE_TIMEOUT_MICROS)
			{
				// Send timeout.
				Task::enable();
				if (SendListener != nullptr)
				{
					SendListener->OnSendComplete(SendResultEnum::SendTimeout);
					SendListener = nullptr;
				}
				State = StateEnum::Done;
			}
			else
			{
				Task::delay(SEND_CHECK_PERIOD_MILLIS);
			}
			break;
		case StateEnum::SendingError:
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::Error);
				SendListener = nullptr;
			}
			Task::enable();
			State = StateEnum::Done;
			break;
		case StateEnum::Done:
			Transceiver->Rx(ServiceListener->GetRxChannel());
			Task::disable();
			break;
		default:
			Task::enable();
			State = StateEnum::Done;
			break;
		}

		if (PendingReceiveSize > 0)
		{
			if (PendingReceiveSize > MaxPacketSize)
			{
				ServiceListener->OnDropped(ReceiveTimestamp, PendingReceiveSize);
			}
			else
			{
				ServiceListener->OnReceived(ReceiveTimestamp, PendingReceiveSize, PendingReceiveRssi);
			}
			PendingReceiveSize = 0;
			Task::enable();
		}

		return true;
	}

public:
	const bool CanSendPacket() const
	{
		return State == StateEnum::Done && Transceiver->TxAvailable();
	}

	void RefreshChannel()
	{
		/// <summary>
		/// If the State is not Done, Service will refresh channel at the end of a transmission/ack.
		/// </summary>
		if (State == StateEnum::Done)
		{
			// Just running the task in Done state will update the channel, but directly calling Driver has less latency.
			Transceiver->Rx(ServiceListener->GetRxChannel());
		}
	}

	void ClearSendRequest()
	{
		if (State != StateEnum::Done)
		{
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::SendTimeout);
			}
#if defined(DEBUG_LOLA)
			Serial.println(F("\tPrevious Send request cancelled."));
#endif
			Task::enable();
			ServiceListener = nullptr;
			State = StateEnum::Done;
		}
	}

	/// <summary>
	/// Using the late call for getting Tx channel, only transmission duration error will affect the precision.
	/// So the MCU packet prepare duration does not influence the Tx channel precision.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	const bool Send(ILinkPacketSender* callback, const uint8_t size, const uint8_t channel)
	{
		if (Transceiver->Tx(RawOutPacket, size, channel))
		{
			SendOutTimestamp = micros();
			SendListener = callback;
			Task::enable();
			State = StateEnum::Sending;

			return true;
		}
		else
		{
			Task::enable();
			State = StateEnum::Done;

			// Turns out we can't send right now, try again later.
			return false;
		}
	}

	/// <summary>
	/// Mock internal "Tx", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="size"></param>
	/// <param name="channel"></param>
	/// <returns></returns>
	const bool MockSend(ILinkPacketSender* callback, const uint8_t size, const uint8_t channel)
	{
		// Mock internal "Tx".
		if ((size > 0 && channel == 0) || (size >= 0))
		{
			// Mock Transceiver call.
			if (((Transceiver->GetTransmitDurationMicros(size) > 0)))
			{
				return true;
			}
		}
		return false;

	}

	/// <summary>
	/// ILoLaTransceiverListener overrides.
	/// </summary>
public:
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		if (PendingReceiveSize > 0)
		{
			// We still have a pending packet, drop this one.
			ServiceListener->OnDropped(receiveTimestamp, packetSize);

			return false;
		}
		else
		{
			// Double buffer input packet, so we don't miss any in the meanwhile.
			PendingReceiveSize = packetSize;
			ReceiveTimestamp = receiveTimestamp;
			PendingReceiveRssi = rssi;

			// Only copy buffer if packet fits, but let task notify listener that a packet was dropped.
			if (packetSize <= MaxPacketSize)
			{
				for (uint_fast8_t i = 0; i < packetSize; i++)
				{
					RawInPacket[i] = data[i];
				}
			}

			Task::enable();

			return true;
		}
	}

	virtual void OnRxLost(const uint32_t receiveTimestamp) final
	{
		ServiceListener->OnLost(receiveTimestamp);
	}

	virtual void OnTx() final
	{
		switch (State)
		{
		case StateEnum::Sending:
			State = StateEnum::SendingSuccess;
			Task::enable();
			break;
		default:
			// Unexpected Sent Event.
			State = StateEnum::SendingError;
			Task::enable();
			break;
		}
	}
};
#endif