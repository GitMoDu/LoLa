// LoLaPacketService.h

#ifndef _LOLA_PACKET_DRIVER_h
#define _LOLA_PACKET_DRIVER_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "LoLaTransceivers/ILoLaTransceiver.h"
#include "IPacketServiceListener.h"


/// <summary>
/// Packet send and receive service, with a single IPacketServiceListener for receive.
/// Interfaces directly with and abstracts ILoLaTransceiver.
/// Properties:
///		Content abstract.
///		Pushes input packets straight to Link buffer, so transceiver is free to receive more while the service processes it.
/// </summary>
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

	static constexpr uint8_t SEND_CHECK_PERIOD_MILLIS = 1;
	static constexpr uint8_t SEND_TIMEOUT_TOLERANCE_MILLIS = 1;

private:
	IPacketServiceListener* ServiceListener;

	uint8_t* RawInPacket = nullptr;
	uint8_t* RawOutPacket = nullptr;

	uint32_t SendOutTimestamp = 0;
	uint8_t SendOutSize = 0;

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
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
		pinMode(RX_TEST_PIN, OUTPUT);
#endif
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
		pinMode(TX_TEST_PIN, OUTPUT);
#endif
	}

	const bool Setup()
	{
		if (RawInPacket != nullptr &&
			RawOutPacket != nullptr &&
			ServiceListener != nullptr &&
			Transceiver != nullptr &&
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
			if (millis() - SendOutTimestamp >
				(SEND_TIMEOUT_TOLERANCE_MILLIS
					+ ((Transceiver->GetTimeToAir(SendOutSize)
						+ Transceiver->GetDurationInAir(SendOutSize)) / ONE_MILLI_MICROS)))
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
			Task::disable();
			Transceiver->Rx(ServiceListener->GetRxChannel());
			break;
		default:
			break;
		}

		if (PendingReceiveSize > 0)
		{
			ServiceListener->OnReceived(ReceiveTimestamp, PendingReceiveSize, PendingReceiveRssi);

			PendingReceiveSize = 0;
			Task::enable();
			return true;
		}

		return State != StateEnum::Done;
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
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, HIGH);
#endif
		if (Transceiver->Tx(RawOutPacket, size, channel))
		{
			SendOutTimestamp = millis();
			SendOutSize = size;
			State = StateEnum::Sending;
			Task::enable();

			return true;
		}
		else
		{
#ifdef TX_TEST_PIN
			digitalWrite(TX_TEST_PIN, LOW);
#endif
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
	/// <summary>
	/// Handles incoming packets.
	/// Checks for busy status and forwards the packet to the listener.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="receiveTimestamp">Accurate timestamp (micros()) of incoming packet start.</param>
	/// <param name="packetSize">[MIN_PACKET_SIZE;MAX_PACKET_TOTAL_SIZE]</param>
	/// <param name="rssi">Normalized RX RSSI [0:255].</param>
	/// <returns>True if packet was successfully consumed. False, try again later.</returns>
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		if (PendingReceiveSize > 0)
		{
			// We still have a pending packet, refuse this one for now.
			return false;
		}

		// Double buffer input packet, so we don't miss any in the meanwhile.
		memcpy((void*)RawInPacket, (const void*)data, (size_t)packetSize);
		PendingReceiveSize = packetSize;
		ReceiveTimestamp = receiveTimestamp;
		PendingReceiveRssi = rssi;

		Task::enable();

		return true;
	}

	virtual void OnTx() final
	{
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
#endif
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