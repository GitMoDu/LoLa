// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <LoLaDefinitions.h>
#include <PacketDriver\BaseLoLaDriver.h>

#include <Link\LoLaLinkPowerBalancer.h>
#include <Link\LoLaLinkChannelManager.h>
#include <Link\LoLaLinkClockSyncer.h>


class LoLaPacketDriver : public BaseLoLaDriver
{
public:
	// Channel manager.
	LoLaLinkChannelManager ChannelManager;

	// Transmit power manager.
	LoLaTransmitPowerManager TransmitPowerManager;

public:
	LoLaPacketDriver(Scheduler* scheduler
		, LoLaPacketDriverSettings* settings)
		: BaseLoLaDriver(settings)
		, TransmitPowerManager(scheduler, this, settings)
		, ChannelManager(scheduler, this, settings, &CryptoEncoder.ChannelTokenGenerator)
	{
	}

	uint8_t GetTransmitPower()
	{
		return TransmitPowerManager.GetTransmitPower();
	}

	uint8_t GetChannel()
	{
		return 10;
		return ChannelManager.GetChannel();
	}

	virtual bool Setup()
	{
		return BaseLoLaDriver::Setup();
	}

public:
	// Radio Driver implementation.

	// To be overloaded but still call OnRadioSetToRX.
	virtual bool SetRadioReceive()
	{
		OnRadioSetToRX();
		return true;
	}

	// To be overloaded.
	virtual bool SetRadioChannel()
	{
		return true;
	}

	// To be overloaded but still restore to RX and call OnRadioSetToRX,
	// regardless if successfull.
	virtual bool RadioTransmit(const uint8_t length)
	{
		OnRadioSetToRX();
		return true;
	}

	// To be overloaded.
	virtual bool SetRadioPower()
	{
		return true;
	}

	virtual void Stop()
	{
		DriverState = DriverActiveState::DriverDisabled;
	}

	virtual void Start()
	{
		Serial.println(F("LoLa Driver Start"));
		SetRadioReceive();
	}

protected:
	void OnRadioSetToRX()
	{
		DriverState = DriverActiveState::ReadyReceiving;
		Serial.println("RX Ready");
	}

private:
	void SendAck(const uint8_t originalHeader, const uint8_t originalId)
	{
		DriverState = DriverActiveState::SendingOutgoing;

		// Set Id to original Packet.

		// Update rolling counter Id.
		OutgoingPacket.SetId(IOInfo.GetNextPacketId());

		// Set header.
		OutgoingPacket.SetHeader(AckDefinition->Header);

		// Payload is original Header and Id.
		OutgoingPacket.GetPayload()[0] = originalHeader;
		OutgoingPacket.GetPayload()[1] = originalId;

		// Encode to outgoing and set MAC.
		// Assume we're ETTM in the future,
		// to remove transmition duration from token clock jitter.
		OutgoingPacket.SetMACCRC(CryptoEncoder.Encode(OutgoingPacket.GetContent(),
			AckDefinition->ContentSize, IOInfo.ETTMillis));

		if (!RadioTransmit(AckDefinition->TotalSize))
		{
			Serial.print(F("Nu nuh: "));
			Serial.println(DriverState);
		}
	}

public:
	const bool SendPacket(PacketDefinition* definition, uint8_t* packetPayload, uint8_t& sentPacketId)
	{
		if (DriverState != DriverActiveState::ReadyReceiving ||
			definition == nullptr)
		{
			return false;
		}

		DriverState = DriverActiveState::SendingOutgoing;

		// Update rolling counter Id, and write it back for those interested.
		OutgoingPacket.SetId(IOInfo.GetNextPacketId());

		// Set header.
		OutgoingPacket.SetHeader(definition->Header);

		// Copy payload.
		for (uint8_t i = 0; i < definition->PayloadSize; i++)
		{
			OutgoingPacket.GetPayload()[i] = packetPayload[i];
		}

		// Encode to outgoing and set MAC.
		// Assume we're ETTM in the future,
		// to remove transmition duration from token clock jitter.
		OutgoingPacket.SetMACCRC(CryptoEncoder.Encode(OutgoingPacket.GetContent(),
			definition->ContentSize, IOInfo.ETTMillis));

		if (RadioTransmit(definition->TotalSize))
		{
			IOInfo.LastValidSent.Millis = millis();
#ifdef DEBUG_LOLA
			IOInfo.TransmitedCount++;
#endif
			return true;
		}
		else
		{
#ifdef DEBUG_LOLA
			IOInfo.TransmitFailuresCount++;
#endif
			Serial.print(F("Nu huh: "));
			Serial.println(DriverState);

			return false;
		}
	}

public:
	// When RF detects incoming packet.
	void OnIncoming(const int16_t rssi, const uint32_t timestampMicros)
	{
#ifdef DEBUG_LOLA
		Serial.print(F("OnIncoming rssi: "));
		Serial.println(rssi);
#endif
		IOInfo.LastReceived.RSSI = rssi;
		IOInfo.LastReceived.Micros = timestampMicros;

		if (DriverState == DriverActiveState::ReadyReceiving)
		{
			DriverState = DriverActiveState::BlockedForIncoming;
		}
		else
		{
			if (DriverState == DriverActiveState::SendingOutgoing)
			{
				// Got packet right after sending one, let's get back to that received packet.
				DriverState = DriverActiveState::BlockedForIncoming;
#ifdef DEBUG_LOLA
				Serial.print(F("Got packet right after sending one"));
				Serial.println(DriverState);
#endif
			}
#ifdef DEBUG_LOLA
			else
			{
				IOInfo.StateCollisionCount++;
				Serial.print(F("Bad state OnIncoming: "));
				Serial.println(DriverState);
			}
#endif
		}
	}

	// When RF has packet to read, driver calls this, after calling OnIncoming()
	void OnReceiveOk(const uint8_t packetLength)
	{
		if (DriverState != DriverActiveState::BlockedForIncoming)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bad state OnReceiveOk: "));
			Serial.println(DriverState);
#endif
			DriverState = DriverActiveState::BlockedForIncoming;
		}

		const uint8_t ContentSize = PacketDefinition::GetContentSize(packetLength);

		//TODO: Replace with a better solution.
		LoLaPacketMap::WrappedDefinition DefinitionWrapper;

		if (packetLength < PacketDefinition::LOLA_PACKET_MIN_PACKET_TOTAL_SIZE ||
			ContentSize > PacketDefinition::LOLA_PACKET_MAX_CONTENT_SIZE)
		{
			// Unaceptable packet size.
#ifdef DEBUG_LOLA
			IOInfo.UnknownCount++;
			Serial.println(F("Unaceptable packet size."));
#endif	
		}
		else if (!CryptoEncoder.Decode(IncomingPacket.GetContent(), ContentSize, IncomingPacket.GetMACCRC(), IOInfo.GetElapsedMillisLastReceived()))
		{
#ifdef DEBUG_LOLA
			IOInfo.RejectedCount++;
			Serial.println(F("MAC/CRC mismatch, packet malformed or not intended for this device."));
#endif	
		}
		else if (LinkInfo.HasLink() && !IOInfo.ValidateNextReceivedCounter(IncomingPacket.GetId()))
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Possible replay attack, Counter Mismatch."));
#endif	
		}
		else if (!PacketMap.TryGetDefinition(IncomingPacket.GetHeader(), &DefinitionWrapper))
		{
#ifdef DEBUG_LOLA
			IOInfo.UnknownCount++;
			Serial.println(F("Packet mapping does not exist."));
#endif	
		}
		else if (AckNotifier.IsAckHeader(DefinitionWrapper.Definition->Header))
		{
			// Is an Ack packet, notify service.
			AckNotifier.NotifyAckReceived(
				IncomingPacket.GetPayload()[0],
				IncomingPacket.GetPayload()[1],
				IOInfo.LastReceived.Micros);

			Serial.println(F("Got Ack!"));
		}
		else
		{
			// Non-Ack-Packet received Ok, let's commit that info really quick.
			IOInfo.PromoteLastReceived();
			const uint8_t Lost = IOInfo.GetCounterLost();
			if (Lost > 0)
			{
				LinkInfo.OnCounterSkip(Lost);
			}

#ifdef DEBUG_LOLA
			// Check for packet collisions.
			CheckReceiveCollision(IOInfo.LastValidReceived.Micros - micros());
#endif

			if (IncomingPacket.NotifyPacketReceived(DefinitionWrapper.Definition, IOInfo.LastValidReceived.Micros) &&
				DefinitionWrapper.Definition->HasAck)
			{
				// If service validation passed and packet has Ack property, send Ack ASAP.
				SendAck(IncomingPacket.GetHeader(), IncomingPacket.GetId());
			}

			if (DriverState != DriverActiveState::SendingOutgoing)
			{
				Serial.println("Forced RX");
				SetRadioReceive();
			}
		}
	}

	// When RF has failed to receive packet.
	void OnReceiveFail()
	{
#ifdef DEBUG_LOLA
		Serial.print(F("OnReceiveFail: "));
		Serial.println(DriverState);
#endif
		SetRadioReceive();
	}
};
#endif