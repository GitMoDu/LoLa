// LoLaPacketService.h

#ifndef _LOLA_PACKET_DRIVER_h
#define _LOLA_PACKET_DRIVER_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaPacketDriver.h>
#include "IPacketServiceListener.h"


/// <summary>
/// Packet send and receive service, with a single IPacketServiceListener for receive.
/// Interfaces directly with and abstracts ILoLaPacketDriver.
/// Optional SendListener event callbacks.
/// Properties:
///		Content abstract.
///		Buffers input packets, so packet driver is free to receive more while the service processes it.
///		ACKs must come in order, otherwise the SendWithAck process is aborted.
/// </summary>
/// <param name="MaxPacketSize">The maximum raw packet size.</param>
template<const uint8_t MaxPacketSize>
class LoLaPacketService : private Task, public virtual ILoLaPacketDriverListener
{
private:
	using SendResultEnum = ILinkPacketSender::SendResultEnum;

	enum StateEnum
	{
		Done,
		Sending,
		SendingWithAck,
		SendingSuccess,
		SendingSuccessAck,
		WaitingForAck,
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

	uint8_t SentHandle = 0;

public:
	ILoLaPacketDriver* Driver = nullptr;

public:
	LoLaPacketService(Scheduler& scheduler,
		IPacketServiceListener* serviceListener,
		ILoLaPacketDriver* driver,
		uint8_t* rawInPacket,
		uint8_t* rawOutPacket)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILoLaPacketDriverListener()
		, ServiceListener(serviceListener)
		, RawInPacket(rawInPacket)
		, RawOutPacket(rawOutPacket)
		, Driver(driver)
	{}

	const bool Setup()
	{
		if (RawInPacket != nullptr &&
			RawOutPacket != nullptr &&
			Driver->DriverSetupListener(this))
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
				SendListener->OnSendComplete(SendResultEnum::Success, SentHandle);
			}
			break;
		case StateEnum::Sending:
		case StateEnum::SendingWithAck:
			if (micros() - SendOutTimestamp > LoLaLinkDefinition::TRANSMIT_BASE_TIMEOUT_MICROS)
			{
				// Send timeout.
				Task::enable();
				if (SendListener != nullptr)
				{
					SendListener->OnSendComplete(SendResultEnum::SendTimeout, SentHandle);
				}
				State = StateEnum::Done;
			}
			else
			{
				Task::delay(SEND_CHECK_PERIOD_MILLIS);
			}
			break;
		case StateEnum::SendingSuccessAck:
			State = StateEnum::WaitingForAck;
			Task::delay(SEND_CHECK_PERIOD_MILLIS);
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::Success, SentHandle);
			}
			break;
		case StateEnum::WaitingForAck:
			if (micros() - SendOutTimestamp > LoLaLinkDefinition::REPLY_BASE_TIMEOUT_MICROS)
			{
				if (SendListener != nullptr)
				{

					SendListener->OnAckTimeout(ReceiveTimestamp, SentHandle);
				}
				Task::enable();
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
				SendListener->OnSendComplete(SendResultEnum::Error, SentHandle);
			}
			Task::enable();
			State = StateEnum::Done;
			break;
		case StateEnum::Done:
			Driver->DriverRx(ServiceListener->GetRxChannel());
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
		return State == StateEnum::Done && Driver->DriverCanTransmit();
	}

	void RefreshChannel()
	{
		/// <summary>
		/// If the State is not Done, Service will refresh channel at the end of a transmission/ack.
		/// </summary>
		if (State == StateEnum::Done)
		{
			// Just running the task in Done state will update the channel, but directly calling Driver has less latency.
			Driver->DriverRx(ServiceListener->GetRxChannel());
		}
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="startTimestamp"></param>
	/// <param name="handle"></param>
	void NotifyAckReceived(const uint32_t startTimestamp, const uint8_t handle)
	{
		bool dropped = true;

		switch (State)
		{
		case StateEnum::SendingWithAck:
#if defined(DEBUG_LOLA)
			Serial.println(F("Packet Send service got an ack before onTransmitted."));
#endif
			// Recover from an early Ack.
			State = StateEnum::WaitingForAck;
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::SendCollision, SentHandle);
			}
			dropped = false;
			NotifyAckReceived(startTimestamp, handle);
			break;
		case StateEnum::WaitingForAck:
			if (SentHandle == handle)
			{
				if (SendListener != nullptr)
				{
					SendListener->OnPacketAckReceived(startTimestamp, SentHandle);
				}
				dropped = false;
			}
			Task::enable();
			State = StateEnum::Done;
			break;
		default:
			break;
		}

		if (dropped)
		{
			ServiceListener->OnAckDropped(startTimestamp);
		}
	}

	void ClearSendRequest()
	{
		if (State != StateEnum::Done)
		{
			if (SendListener != nullptr)
			{
				SendListener->OnSendComplete(SendResultEnum::SendTimeout, SentHandle);
			}
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
	/// <param name="handle"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	const bool Send(ILinkPacketSender* callback, const uint8_t handle, const uint8_t size, const uint8_t channel)
	{
		if (Driver->DriverTx(RawOutPacket, size, channel))
		{
			SendOutTimestamp = micros();
			SendListener = callback;
			SentHandle = handle;
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
	/// Mock internal "DriverTx", 
	/// for calibration/testing purposes.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="handle"></param>
	/// <param name="size"></param>
	/// <param name="channel"></param>
	/// <returns></returns>
	const bool MockSend(ILinkPacketSender* callback, const uint8_t handle, const uint8_t size, const uint8_t channel)
	{
		// Mock internal "DriverTx".
		if ((size > 0 && channel == 0) || (size >= 0))
		{
			// Mock driver call.
			if (((Driver->GetTransmitDurationMicros(size) > 0) || handle == 0) || handle > 0)
			{
				return true;
			}
		}
		return false;

	}

	/// <summary>
	/// Using the late call for getting Tx channel, only transmission duration error will affect the precision.
	/// So the MCU packet prepare duration does not influence the Tx channel precision.
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="handle"></param>
	/// <param name="data"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	const bool SendWithAck(ILinkPacketSender* callback, const uint8_t handle, uint8_t* data, const uint8_t size, const uint8_t channel)
	{
		if (Driver->DriverTx(data, size, channel))
		{
			SendOutTimestamp = micros();
			State = StateEnum::SendingWithAck;
			SendListener = callback;
			SentHandle = handle;
			Task::enable();

			return true;
		}
		else
		{
			// Turns out we can't send right now, try again later.
			if (callback != nullptr)
			{
				callback->OnSendComplete(SendResultEnum::SendCollision, handle);
			}

			return false;
		}
	}

	/// <summary>
	/// ILoLaPacketDriverListener overrides.
	/// </summary>
public:
	virtual void OnReceived(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		if (PendingReceiveSize > 0)
		{
			// We still have a pending packet, drop this one.
			ServiceListener->OnDropped(receiveTimestamp, packetSize);
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
		}
	}

	virtual void OnReceiveLost(const uint32_t receiveTimestamp) final
	{
		ServiceListener->OnLost(receiveTimestamp);
	}

	virtual void OnSent(const bool success) final
	{
		switch (State)
		{
		case StateEnum::Sending:
			State = StateEnum::SendingSuccess;
			Task::enable();
			break;
		case StateEnum::SendingWithAck:
			State = StateEnum::SendingSuccessAck;
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