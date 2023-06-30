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
/// Properties:
///		Content abstract.
///		Buffers input packets, so transceiver is free to receive more while the service processes it.
/// </summary>
/// <param name="MaxPacketSize">The maximum raw packet size.</param>
template<const uint8_t MaxPacketSize>
class LoLaPacketService : private Task, public virtual ILoLaTransceiverListener
{
private:
	using SendResultEnum = IPacketServiceListener::SendResultEnum;

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

	uint32_t SendOutTimestamp = 0;

	volatile uint32_t ReceiveTimestamp = 0;
	volatile uint8_t PendingReceiveSize = 0;
	volatile uint8_t PendingReceiveRssi = 0;

	volatile StateEnum State = StateEnum::Done;

public:
	ILoLaTransceiver* Transceiver;

public:
	LoLaPacketService(Scheduler& scheduler,
		IPacketServiceListener* serviceListener,
		ILoLaTransceiver* transceiver,
		uint8_t* rawInPacket,
		uint8_t* rawOutPacket)
		: ILoLaTransceiverListener()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ServiceListener(serviceListener)
		, RawInPacket(rawInPacket)
		, RawOutPacket(rawOutPacket)
		, Transceiver(transceiver)
	{}

	const bool Setup()
	{
		if (RawInPacket != nullptr &&
			RawOutPacket != nullptr &&
			ServiceListener != nullptr &&
			Transceiver->SetupListener(this))
		{
			return true;
		}
#if defined(DEBUG_LOLA)
		else
		{
			Serial.println(F("PacketService setup failed."));
		}
#endif

		return false;
	}

	virtual bool Callback() final
	{
		switch (State)
		{
		case StateEnum::SendingSuccess:
			State = StateEnum::Done;
			Task::enable();
			ServiceListener->OnSendComplete(SendResultEnum::Success);
			break;
		case StateEnum::Sending:
			if (micros() - SendOutTimestamp > LoLaLinkDefinition::TRANSMIT_BASE_TIMEOUT_MICROS)
			{
				// Send timeout.
				State = StateEnum::Done;
				Task::enable();
				ServiceListener->OnSendComplete(SendResultEnum::SendTimeout);
			}
			else
			{
				Task::delay(SEND_CHECK_PERIOD_MILLIS);
			}
			break;
		case StateEnum::SendingError:
			Task::enable();
			State = StateEnum::Done;
			ServiceListener->OnSendComplete(SendResultEnum::Error);
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
			// Just running the task in Done state will update the channel on the next task allback.
			Task::enable();
		}
	}

	void ClearSendRequest()
	{
		if (State != StateEnum::Done)
		{
			ServiceListener->OnSendComplete(SendResultEnum::SendTimeout);

#if defined(DEBUG_LOLA)
			Serial.println(F("\tPrevious Send request cancelled."));
#endif
			Task::enable();
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
	const bool Send(const uint8_t size, const uint8_t channel)
	{
		if (Transceiver->Tx(RawOutPacket, size, channel))
		{
			SendOutTimestamp = micros();
			State = StateEnum::Sending;
			Task::enable();

			return true;
		}
		else
		{
			State = StateEnum::Done;
			Task::enable();

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
	const bool MockSend(const uint8_t size, const uint8_t channel)
	{
		// Mock internal "Tx".
		if ((size > 0 && channel == 0) || (size >= 0))
		{
			// Mock Transceiver call.
			if (((Transceiver->GetTimeToAir(size) > 0)))
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