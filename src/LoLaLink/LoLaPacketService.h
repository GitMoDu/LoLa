// LoLaPacketService.h

#ifndef _LOLA_PACKET_SERVICE_h
#define _LOLA_PACKET_SERVICE_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <PacketDriver\ILoLa.h>
#include <LoLaLink\PacketDefinition.h>
#include <LoLaLink\RollingCounters.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


template<const uint8_t MaxPacketSize>
class LoLaPacketService : protected Task, public ILoLaPacketListener
{
private:
	enum StateEnum
	{
		Done,
		Sending,
		SendingWithAck
	};

	static const uint32_t SEND_CHECK_PERIOD_MILLIS = 1;

	RollingCounters PacketCounter;


	volatile StateEnum State = StateEnum::Done;

protected:
	uint8_t OutMessage[MaxPacketSize];
	uint8_t InMessage[MaxPacketSize];

	ILoLaPacketDriver* Driver = nullptr;

	uint32_t SentTimestamp = 0;
	uint32_t ReceiveTimeTimestamp = 0;

	uint8_t SentSize = 0;
	uint8_t PendingReceiveSize = 0;

protected:
	// Runs when no work is pending.
	virtual void OnService()
	{
		Task::disable();
	}

	// To be overriden.
	virtual void OnReceive(const uint8_t contentSize) { }
	virtual void OnAckFailed() { }


public:
	LoLaPacketService(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: Task(0, TASK_FOREVER, scheduler, false)
		, PacketCounter()
	{
	}

	virtual const bool Setup()
	{
		if (Driver->DriverSetup(this))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

protected:
	void SetOutMac(const uint32_t mac)
	{
		// Set MAC/CRC.
		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0] = mac & 0xFF;
		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1] = (mac >> 8) & 0xFF;
		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2] = (mac >> 16) & 0xFF;
		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3] = (mac >> 24) & 0xFF;
	}

	void SetOutId()
	{
		// Update rolling counter Id.
		OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX] = PacketCounter.GetLocalNext();
	}

	const bool CanSendPacket()
	{
		return State == StateEnum::Done && Driver->DriverCanTransmit();
	}

	// SetOutMac and SetOutId must be called before.
	const bool SendPacket(const uint8_t size, const bool hasAck)
	{
		if (State == StateEnum::Done &&
			size <= MaxPacketSize &&
			Driver->DriverCanTransmit())
		{
			if (Driver->DriverTransmit(OutMessage, size))
			{
				SentSize = size;

				if (hasAck)
				{
					SentTimestamp = micros();
					State = StateEnum::SendingWithAck;
				}
				else
				{
					State = StateEnum::Sending;
				}

				// Update statistics.
				//Sent++;

				return true;
			}
			else
			{
				SentTimestamp = 0;
				return false;
			}
		}
		else
		{
			// Turns out we can send right now, try again later.
			return false;
		}
	}

	bool Callback()
	{
		if (PendingReceiveSize > 0)
		{
			const uint8_t contentSize = PacketDefinition::GetContentSize(PendingReceiveSize);

			OnReceive(contentSize);

			PendingReceiveSize = 0;
		}
		else
		{
			switch (State)
			{
			case StateEnum::Sending:
				if (micros() - SentTimestamp > Driver->GetTransmitTimeoutMicros(SentSize))
				{
					// Send timeout.
					State = StateEnum::Done;
				}
				Task::enable();
				break;
			case StateEnum::SendingWithAck:
				if (micros() - SentTimestamp + Driver->GetTransmitTimeoutMicros(SentSize) > Driver->GetReplyTimeoutMicros(SentSize))
				{
					// Send timeout.
					State = StateEnum::Done;
					OnAckFailed();
				}
				Task::enable();
				break;
			case StateEnum::Done:
				OnService();
				break;
			default:
				State = StateEnum::Done;
				break;
			}
		}

		return true;
	}

	// ILoLaPacketListener overrides.
	virtual const bool OnPacketReceived(uint8_t* data, const uint32_t startTimestamp, const uint8_t packetSize)
	{
		if (PendingReceiveSize > 0)
		{
			// We still have a pending packet, drop this one.
			return false;
		}
		else
		{
			// Double buffer input packet, so we don't miss any in the meanwhile.
			PendingReceiveSize = packetSize;
			ReceiveTimeTimestamp = startTimestamp;
			for (uint8_t i = 0; i < packetSize; i++)
			{
				InMessage[i] = data[i];
			}

			Task::enable();
		}
	}

	virtual void OnPacketSent()
	{
		switch (State)
		{
		case StateEnum::Sending:
		case StateEnum::SendingWithAck:
			State = StateEnum::Done;
			Task::enable();
			break;
		default:
			break;
		}
	}

	virtual void OnPacketLost(const uint32_t startTimestamp) {}
};
#endif