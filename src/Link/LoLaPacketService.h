// LoLaPacketService.h

#ifndef _LOLA_PACKET_SERVICE_h
#define _LOLA_PACKET_SERVICE_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <PacketDriver\ILoLa.h>
#include <Link\PacketDefinition.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

struct RollingCountersStruct
{
	uint8_t Local = 0;
	uint8_t LastReceivedCounter = 0;
	uint8_t LastReceivedDelta = 0;
	bool LastReceivedPresent = false;

	uint8_t CounterSkipped = 0;

	const uint8_t GetLocalNext()
	{
		Local = (Local + 1) % UINT8_MAX;

		return Local;
	}

	const bool PushReceivedCounter(const uint8_t receivedPacketCounter)
	{
		// Packet counter can rollover and we wouldn't notice.
		CounterSkipped = receivedPacketCounter - LastReceivedCounter;

		if (CounterSkipped != 0)
		{
			LastReceivedCounter = receivedPacketCounter;

			return true;
		}
		else
		{
			return false;
		}
	}

	void SetReceivedCounter(const uint8_t receivedPacketCounter)
	{
		LastReceivedCounter = receivedPacketCounter;
		LastReceivedPresent = true;
		CounterSkipped = 0;
	}

	void Reset()
	{
		CounterSkipped = 0;
		Local = 0;
		LastReceivedPresent = false;
	}

	const uint8_t GetCounterLost()
	{
		if (CounterSkipped > 0)
		{
			return CounterSkipped - 1;
		}
		else
		{
			return 0;
		}
	}
};


template<const uint8_t MaxPacketSize>
class LoLaPacketService : public Task, public ILoLaPacketListener
{
private:
	// Crypto encoder.
	//LoLaCryptoEncoder CryptoEncoder;


public:
	uint8_t OutMessage[MaxPacketSize];
	uint8_t InMessage[MaxPacketSize];

	ILoLaPacketDriver* Driver = nullptr;

public:
	LoLaPacketService(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: Task(0, TASK_FOREVER, scheduler, false)
		, Driver(driver)
	{
	}

	virtual const bool Setup()
	{
		if (Driver->DriverSetup(this))
		{
			return true;
		}

		return false;
	}
};


template<const uint8_t MaxPacketSize>
class LoLaPacketSendReceive : public LoLaPacketService<MaxPacketSize>
{
private:
	enum StateEnum
	{
		Done,
		Sending,
		SendingWithAck
	};

	static const uint32_t SEND_CHECK_PERIOD_MILLIS = 1;

	RollingCountersStruct RollingCounters;


	volatile StateEnum State = StateEnum::Done;

protected:
	using LoLaPacketService<MaxPacketSize>::Driver;
	using LoLaPacketService<MaxPacketSize>::OutMessage;
	using LoLaPacketService<MaxPacketSize>::InMessage;


	uint32_t SentTimestamp = 0;

	uint32_t ReceiveTimeTimestamp = 0;

	uint8_t PendingReceiveSize = 0;
	uint8_t SentSize = 0;

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
	LoLaPacketSendReceive(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaPacketService<MaxPacketSize>(scheduler, driver)
	{
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
		OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX] = RollingCounters.GetLocalNext();
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


template<const uint8_t MaxPacketSize>
class LoLaCryptoSendReceive : public LoLaPacketSendReceive<MaxPacketSize>
{
public:
	using LoLaPacketSendReceive<MaxPacketSize>::InMessage;
	using LoLaPacketSendReceive<MaxPacketSize>::OutMessage;
	//using LoLaPacketSendReceive<MaxPacketSize>::ReceiveTimeTimestamp;
	//using LoLaPacketSendReceiv\\\\\\\\\\\\\e<MaxPacketSize>::CanSendPacket;
	//using LoLaPacketSendReceive<MaxPacketSize>::SendPacket;

// Runtime Flags.
	bool TokenHopEnabled = false;
	bool EncryptionEnabled = false;

	LoLaCryptoEncoder CryptoEncoder;

	/*TOTPSecondsTokenGenerator LinkTokenGenerator;
	TOTPMillisecondsTokenGenerator ChannelTokenGenerator;*/

public:
	LoLaCryptoSendReceive(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaPacketSendReceive<MaxPacketSize>(scheduler, driver)
		, CryptoEncoder()
		//, LinkTokenGenerator(syncedClock)
		//, ChannelTokenGenerator(syncedClock, LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS)
	{
	}

	virtual const bool Setup()
	{
		if (LoLaPacketSendReceive<MaxPacketSize>::Setup())
		{
			CryptoEncoder.Setup();
			return true;
		}
		else
		{
			return false;
		}
	}

protected:
	void EncodeOutputContent(const uint32_t token, const uint8_t contentSize)
	{
		uint32_t mac;
		if (EncryptionEnabled)
		{
			mac = CryptoEncoder.Encode(
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
		else
		{
			mac = CryptoEncoder.NoEncode(
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}

		// Set MAC/CRC.
		SetOutMac(mac);
	}

	const bool DecodeInputContent(const uint32_t token, const uint8_t contentSize)
	{
		uint32_t macCrc = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0];
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1] << 8;
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2] << 16;
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3] << 24;

		if (EncryptionEnabled)
		{
			return CryptoEncoder.Decode(
				macCrc,
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
		else
		{
			return CryptoEncoder.NoDecode(
				macCrc,
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
	}
};
#endif