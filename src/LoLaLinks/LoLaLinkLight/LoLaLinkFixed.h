// LoLaLinkFixed.h
// Using a fixed key.
// Simple keep alive link with fixed encryption.
#ifndef _LOLA_NO_LINK_
#define _LOLA_NO_LINK_

#include "Abstract\AbstractLoLaLinkHost.h"
#include "Abstract\AbstractLoLaLinkRemote.h"


//
//
//template<const uint8_t MaxPacketReceiveListeners = 10,
//	const uint8_t MaxLinkListeners = 10>
//class LoLaLinkFixedHost final
//	: public AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
//{
//private:
//	using BaseClass = AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;
//
//	using Unlinked = LoLaLinkDefinition::Unlinked;
//	using Linking = LoLaLinkDefinition::Linking;
//	using Linked = LoLaLinkDefinition::Linked;
//
//public:
//	using BaseClass::CYPHER_KEY_SIZE;
//
//protected:
//	using BaseClass::Driver;
//	using BaseClass::SyncClock;
//	using BaseClass::PreLinkResendDelayMillis;
//	using BaseClass::OutPacket;
//	using BaseClass::PacketService;
//	using BaseClass::LinkStage;
//	using BaseClass::SecretKey;
//
//
//	using BaseClass::SetSessionId;
//	using BaseClass::SetInAddress;
//	using BaseClass::SetOutAddress;
//
//	using BaseClass::GetRandomLong;
//	using BaseClass::GetRandomCrypto;
//	using BaseClass::SendPacketWithAck;
//
//	using BaseClass::LINKING_STAGE_TIMEOUT;
//
//	static const uint32_t PARTNER_SEARCH_TIMEOUT = 500;
//
//
//	uint8_t* EncryptionKey;
//	uint32_t PreLinkPacketSchedule = 0;
//
//	uint32_t SentCount = 0;
//	uint32_t ReceivedCount = 0;
//	uint32_t DroppedCount = 0;
//
//	uint8_t SwitchOverHandle = 0;
//	uint8_t LooseHandle = 0;
//
//	bool PartnerFound = false;
//	bool PartnerAck = false;
//
//public:
//	LoLaLinkFixedHost(Scheduler& scheduler,
//		ILoLaPacketDriver* driver,
//		ISynchronizedClock* syncClock,
//		IDuplex* duplex,
//		IChannelHopConfiguration* hop,
//		uint8_t* publicKey,
//		uint8_t* privateKey,
//		uint8_t* accessPassword)
//		: BaseClass(scheduler, driver, syncClock, duplex, hop, publicKey, privateKey, accessPassword)
//		, EncryptionKey(accessPassword)
//	{
//		SetFixedSharedKey();
//	}
//
//protected:
//	virtual void LogChannel(const uint8_t channel) {}
//	//virtual void LogHop() {}
//
//	void SetFixedSharedKey()
//	{
//		for (uint8_t i = 0; i < CYPHER_KEY_SIZE; i++)
//		{
//			if (i < LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE)
//			{
//				SecretKey[i] = EncryptionKey[i];
//			}
//			else
//			{
//				SecretKey[i] = 0;
//			}
//		}
//	}
//
//public:
//	virtual void OnPacketAckReceived(const uint32_t startTimestamp, const uint8_t handle) final
//	{
//		bool accepted = false;
//		switch (LinkStage)
//		{
//		case LinkStageEnum::AwaitingLink:
//			if (SwitchOverHandle == handle)
//			{
//				accepted = true;
//				PrintPre();
//				Serial.println(F("Got Linking SwitchOver."));
//				UpdateLinkStage(LinkStageEnum::Linking);
//			}
//			break;
//		case LinkStageEnum::Linking:
//			if (SwitchOverHandle == handle)
//			{
//				accepted = true;
//				PrintPre();
//				Serial.println(F("Got Link SwitchOver."));
//				UpdateLinkStage(LinkStageEnum::Linked);
//			}
//			break;
//		default:
//			break;
//		}
//
//		if (!accepted)
//		{
//			PrintPre();
//			Serial.println(F("SwitchOverHandle Rejected."));
//		}
//	}
//
//protected:
//	virtual const bool OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
//	{
//		if (LinkStage != LinkStageEnum::Disabled)
//		{
//			switch (port)
//			{
//			case Unlinked::PORT:
//				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
//				{
//				case Unlinked::PartnerDiscovery::SUB_HEADER:
//					if (LinkStage == LinkStageEnum::AwaitingLink
//						&& payloadSize == Unlinked::PartnerDiscovery::PAYLOAD_SIZE)
//					{
//						PartnerFound = true;
//						PartnerAck = payload[Unlinked::PartnerDiscovery::PAYLOAD_PARTNER_ACKNOWLEDGED_INDEX] != 0;
//
//						PrintPre();
//						Serial.print(F("Got Discovery "));
//						if (PartnerAck)
//						{
//							Serial.println(F("(true)"));
//						}
//						else
//						{
//							Serial.println(F("(false)"));
//						}
//
//						Task::enable();
//					}
//					break;
//				default:
//					break;
//				}
//				break;
//			case Linking::PORT:
//				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
//				{
//				default:
//					break;
//				}
//				break;
//			default:
//				break;
//			}
//			//return BaseClass::OnUnlinkedPacketReceived(startTimestamp, payload, port, payloadSize);
//		}
//
//		return false;
//	}
//
//	virtual void Owner() final
//	{
//		PrintPre();
//	}
//
//	void PrintPre()
//	{
//		Serial.print(micros());
//		Serial.print(F("\t[H] "));
//	}
//
//public:
//	virtual const bool Start() final
//	{
//		if (LinkStage == LinkStageEnum::Disabled)
//		{
//			UpdateLinkStage(LinkStageEnum::Booting);
//
//			return true;
//		}
//		else
//		{
//			Task::enableIfNot();
//
//			return false;
//		}
//	}
//
//	virtual const bool Stop() final
//	{
//		const bool driverOk = Driver->DriverStop();
//		UpdateLinkStage(LinkStageEnum::Disabled);
//
//		return driverOk;
//	}
//
//protected:
//	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
//	{
//		BaseClass::UpdateLinkStage(linkStage);
//
//		switch (linkStage)
//		{
//		case LinkStageEnum::Disabled:
//			break;
//		case LinkStageEnum::AwaitingLink:
//			SetSessionId(GetRandomCrypto());
//
//			// Reset the local link clock to a random value.
//			SyncClock->SetSeconds(GetRandomLong());
//			PartnerFound = false;
//			PartnerAck = false;
//			PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::PartnerDiscovery::PAYLOAD_SIZE);
//			break;
//		case LinkStageEnum::Linking:
//			// Reset the local link clock to a random value.
//			SyncClock->SetSeconds(GetRandomLong());
//			SwitchOverHandle = 0;
//			PreLinkPacketSchedule = 0;
//			break;
//		case LinkStageEnum::Linked:
//			break;
//		default:
//			break;
//		}
//
//
//
//		PrintPre();
//		Serial.print(F("-LinkStage: "));
//		switch (linkStage)
//		{
//		case LinkStageEnum::Disabled:
//			Serial.println(F("Disabled."));
//			break;
//		case LinkStageEnum::Booting:
//			Serial.println(F("Booting."));
//			break;
//		case LinkStageEnum::AwaitingLink:
//			Serial.println(F("AwaitingLink."));
//			break;
//		case LinkStageEnum::Linking:
//			Serial.println(F("Linking."));
//			break;
//		case LinkStageEnum::Linked:
//			Serial.println(F("Linked."));
//			break;
//		default:
//			break;
//		}
//	}
//
//protected:
//	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
//	{
//		if (stateElapsed > PARTNER_SEARCH_TIMEOUT)
//		{
//			PrintPre();
//			Serial.println(F("OnAwaitingLink timeout."));
//			//UpdateLinkStage(LinkStageEnum::Disabled);
//			UpdateLinkStage(LinkStageEnum::AwaitingLink);
//		}
//		else if (PartnerAck)
//		{
//			if ((millis() > PreLinkPacketSchedule) && PacketService.CanSendPacket())
//			{
//				OutPacket.SetPort(Unlinked::PORT);
//				OutPacket.Payload[SubHeaderDefinition::SUB_HEADER_INDEX] = Unlinked::LinkingSwitchOver::SUB_HEADER;
//				if (SendPacketWithAck(this, SwitchOverHandle, OutPacket.Content, Unlinked::LinkingSwitchOver::PAYLOAD_SIZE))
//				{
//					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::LinkingSwitchOver::PAYLOAD_SIZE);
//					PrintPre();
//					Serial.println(F("Sent Linking SwitchOver."));
//				}
//				else
//				{
//					Task::enable();
//				}
//			}
//			else
//			{
//				Task::enable();
//			}
//		}
//		else if ((millis() > PreLinkPacketSchedule) && PacketService.CanSendPacket())
//		{
//			TrySendDiscovery();
//		}
//		else
//		{
//			Task::enable();
//		}
//	}
//
//	virtual void OnLinking(const uint32_t stateElapsed) final
//	{
//		if (stateElapsed > LINKING_STAGE_TIMEOUT)
//		{
//			PrintPre();
//			Serial.println(F("Linking timed out!"));
//			UpdateLinkStage(LinkStageEnum::AwaitingLink);
//		}
//
//		if ((millis() > PreLinkPacketSchedule) && PacketService.CanSendPacket())
//		{
//			OutPacket.SetPort(Linking::PORT);
//			OutPacket.Payload[SubHeaderDefinition::SUB_HEADER_INDEX] = Linking::LinkSwitchOver::SUB_HEADER;
//			if (SendPacketWithAck(this, SwitchOverHandle, OutPacket.Content, Linking::LinkSwitchOver::PAYLOAD_SIZE))
//			{
//				PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::LinkSwitchOver::PAYLOAD_SIZE);
//				PrintPre();
//				Serial.println(F("Sent Link SwitchOver"));
//			}
//			else
//			{
//				Task::enable();
//			}
//		}
//		else
//		{
//			Task::enable();
//		}
//	}
//
//private:
//	void TrySendDiscovery()
//	{
//		OutPacket.SetPort(Unlinked::PORT);
//		OutPacket.Payload[SubHeaderDefinition::SUB_HEADER_INDEX] = Unlinked::PartnerDiscovery::SUB_HEADER;
//		OutPacket.Payload[Unlinked::PartnerDiscovery::PAYLOAD_PARTNER_ACKNOWLEDGED_INDEX] = PartnerFound;
//
//		if (this->SendPacket(nullptr, LooseHandle, OutPacket.Content, Unlinked::PartnerDiscovery::PAYLOAD_SIZE))
//		{
//			PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::PartnerDiscovery::PAYLOAD_SIZE);
//			PrintPre();
//			Serial.print(F("Sent Discovery "));
//			if (PartnerFound)
//			{
//				Serial.println(F("(true)"));
//			}
//			else
//			{
//				Serial.println(F("(false)"));
//			}
//			Task::enable();
//		}
//		else
//		{
//			Task::enableDelayed(1);
//		}
//	}
//
//protected:
//	virtual void OnEvent(const PacketEventEnum packetEvent) final
//	{
//		switch (packetEvent)
//		{
//		case PacketEventEnum::Received:
//			ReceivedCount++;
//			break;
//		case PacketEventEnum::ReceivedAck:
//			ReceivedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedAck:
//			break;
//		case PacketEventEnum::ReceiveRejectedCounter:
//			break;
//		case PacketEventEnum::ReceiveRejectedDriver:
//			DroppedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedLink:
//			DroppedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedMacCrc:
//			break;
//		case PacketEventEnum::SendAckFailed:
//			break;
//		case PacketEventEnum::SendCollision:
//			break;
//		case PacketEventEnum::Sent:
//			SentCount++;
//			break;
//		case PacketEventEnum::SentAck:
//			SentCount++;
//			break;
//		case PacketEventEnum::SentWithAck:
//			SentCount++;
//			break;
//		default:
//			break;
//		}
//
//		switch (packetEvent)
//		{
//		case PacketEventEnum::Received:
//			break;
//		case PacketEventEnum::ReceivedAck:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceivedAck"));
//			break;
//		case PacketEventEnum::ReceiveRejectedAck:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedAck"));
//			break;
//		case PacketEventEnum::ReceiveRejectedCounter:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedCounter"));
//			break;
//		case PacketEventEnum::ReceiveRejectedDriver:
//			Serial.println(F("@Link Event: ReceiveRejectedDriver"));
//			break;
//		case PacketEventEnum::ReceiveRejectedLink:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejected Link Busy"));
//			break;
//		case PacketEventEnum::ReceiveRejectedMacCrc:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedMacCrc"));
//			break;
//		case PacketEventEnum::SendAckFailed:
//			PrintPre();
//			Serial.println(F("@Link Event: SendAckFailed"));
//			break;
//		case PacketEventEnum::SendCollision:
//			PrintPre();
//			Serial.println(F("@Link Event: SendCollision"));
//			break;
//		case PacketEventEnum::Sent:
//			break;
//		case PacketEventEnum::SentAck:
//			PrintPre();
//			Serial.println(F("@Link Event: SentAck"));
//			break;
//		case PacketEventEnum::SentWithAck:
//			PrintPre();
//			Serial.println(F("@Link Event: SentWithAck"));
//			break;
//		default:
//			break;
//		}
//	}
//};
//
//
//template<const uint8_t MaxPacketReceiveListeners = 10,
//	const uint8_t MaxLinkListeners = 10>
//class LoLaLinkFixedRemote final
//	: public AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
//{
//private:
//	using BaseClass = AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;
//
//	using Unlinked = LoLaLinkDefinition::Unlinked;
//	using Linking = LoLaLinkDefinition::Linking;
//	using Linked = LoLaLinkDefinition::Linked;
//
//public:
//	using BaseClass::CYPHER_KEY_SIZE;
//
//protected:
//	using BaseClass::Driver;
//	using BaseClass::SyncClock;
//	using BaseClass::PreLinkResendDelayMillis;
//	using BaseClass::OutPacket;
//	using BaseClass::PacketService;
//	using BaseClass::LinkStage;
//	using BaseClass::SecretKey;
//
//
//	using BaseClass::SetSessionId;
//	using BaseClass::SetInAddress;
//	using BaseClass::SetOutAddress;
//
//	using BaseClass::GetRandomLong;
//	using BaseClass::GetRandomCrypto;
//	using BaseClass::SendPacketWithAck;
//
//	using BaseClass::LINKING_STAGE_TIMEOUT;
//
//	static const uint32_t PARTNER_SEARCH_TIMEOUT = 500;
//
//	uint8_t* EncryptionKey;
//
//	uint32_t PreLinkPacketSchedule = 0;
//
//	uint32_t SentCount = 0;
//	uint32_t ReceivedCount = 0;
//	uint32_t DroppedCount = 0;
//
//	uint8_t SwitchOverHandle = 0;
//	uint8_t LooseHandle = 0;
//
//	bool PartnerFound = false;
//	bool PartnerAck = false;
//
//private:
//
//public:
//	LoLaLinkFixedRemote(Scheduler& scheduler,
//		ILoLaPacketDriver* driver,
//		ISynchronizedClock* syncClock,
//		IDuplex* duplex,
//		IChannelHopConfiguration* hop,
//		uint8_t* publicKey,
//		uint8_t* privateKey,
//		uint8_t* accessPassword)
//		: BaseClass(scheduler, driver, syncClock, duplex, hop, publicKey, privateKey, accessPassword)
//		, EncryptionKey(accessPassword)
//	{
//		SetFixedSharedKey();
//	}
//
//protected:
//	//virtual void LogChannel(const uint8_t channel) {}
//	virtual void LogHop() {}
//
//	void SetFixedSharedKey()
//	{
//		for (uint8_t i = 0; i < CYPHER_KEY_SIZE; i++)
//		{
//			if (i < LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE)
//			{
//				SecretKey[i] = EncryptionKey[i];
//			}
//			else
//			{
//				SecretKey[i] = 0;
//			}
//		}
//	}
//
//protected:
//	virtual void OnAckSent(const uint8_t port, const uint8_t id) final
//	{
//		switch (LinkStage)
//		{
//		case LinkStageEnum::Disabled:
//			break;
//		case LinkStageEnum::Booting:
//			break;
//		case LinkStageEnum::AwaitingLink:
//			if (port == Unlinked::PORT && SwitchOverHandle == id)
//			{
//				PrintPre();
//				Serial.println(F("On Ack Send Linking!"));
//				UpdateLinkStage(LinkStageEnum::Linking);
//			}
//			break;
//		case LinkStageEnum::Linking:
//			if (port == Linking::PORT && SwitchOverHandle == id)
//			{
//				PrintPre();
//				Serial.println(F("On Ack Send Linked!"));
//				UpdateLinkStage(LinkStageEnum::Linked);
//			}
//			break;
//		case LinkStageEnum::Linked:
//			break;
//		default:
//			break;
//		}
//	}
//
//	virtual const bool OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
//	{
//		if (LinkStage != LinkStageEnum::Disabled)
//		{
//			switch (port)
//			{
//			case Unlinked::PORT:
//				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
//				{
//				case Unlinked::PartnerDiscovery::SUB_HEADER:
//					if (LinkStage == LinkStageEnum::AwaitingLink
//						&& payloadSize == Unlinked::PartnerDiscovery::PAYLOAD_SIZE)
//					{
//						PartnerFound = true;
//						PartnerAck = payload[Unlinked::PartnerDiscovery::PAYLOAD_PARTNER_ACKNOWLEDGED_INDEX] != 0;
//
//						//SetSessionId(payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);
//						SetSessionId(0);
//
//						PrintPre();
//						Serial.print(F("Got Discovery "));
//						if (PartnerAck)
//						{
//							Serial.println(F("(true)"));
//						}
//						else
//						{
//							Serial.println(F("(false)"));
//						}
//
//						Task::enable();
//					}
//					break;
//				case Unlinked::LinkingSwitchOver::SUB_HEADER:
//					if (LinkStage == LinkStageEnum::AwaitingLink
//						&& payloadSize == Unlinked::LinkingSwitchOver::PAYLOAD_SIZE)
//					{
//						PartnerAck = true;
//						SwitchOverHandle = counter;
//						PrintPre();
//						Serial.println(F("Got Linking SwitchOver."));
//
//						return true;
//					}
//					break;
//				default:
//					break;
//				}
//				break;
//			case Linking::PORT:
//				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
//				{
//				case Linking::LinkSwitchOver::SUB_HEADER:
//					if (LinkStage == LinkStageEnum::Linking
//						&& payloadSize == Linking::LinkSwitchOver::PAYLOAD_SIZE)
//					{
//						PrintPre();
//						Serial.println(F("Got Link SwitchOver"));
//						SwitchOverHandle = counter;
//						Task::enable();
//
//						return true;
//					}
//					break;
//				default:
//					break;
//				}
//				break;
//			default:
//				break;
//			}
//			//return BaseClass::OnPacketReceivedInactive(startTimestamp, payload, port, payloadSize);
//		}
//
//		return false;
//	}
//
//	virtual void Owner() final
//	{
//		PrintPre();
//	}
//
//	void PrintPre()
//	{
//		Serial.print(micros());
//		Serial.print(F("\t[R] "));
//	}
//
//
//protected:
//	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
//	{
//		BaseClass::UpdateLinkStage(linkStage);
//
//		switch (linkStage)
//		{
//		case LinkStageEnum::Disabled:
//			break;
//		case LinkStageEnum::AwaitingLink:
//			PartnerFound = false;
//			PartnerAck = false;
//			PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::PartnerDiscovery::PAYLOAD_SIZE);
//			break;
//		case LinkStageEnum::Linking:
//			SwitchOverHandle = 0;
//			PreLinkPacketSchedule = 0;
//			break;
//		case LinkStageEnum::Linked:
//			break;
//		default:
//			break;
//		}
//
//
//
//		PrintPre();
//		Serial.print(F("-LinkStage: "));
//		switch (linkStage)
//		{
//		case LinkStageEnum::Disabled:
//			Serial.println(F("Disabled."));
//			break;
//		case LinkStageEnum::Booting:
//			Serial.println(F("Booting."));
//			break;
//		case LinkStageEnum::AwaitingLink:
//			Serial.println(F("AwaitingLink."));
//			break;
//		case LinkStageEnum::Linking:
//			Serial.println(F("Linking."));
//			break;
//		case LinkStageEnum::Linked:
//			Serial.println(F("Linked."));
//			break;
//		default:
//			break;
//		}
//	}
//
//protected:
//	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
//	{
//		if (stateElapsed > PARTNER_SEARCH_TIMEOUT)
//		{
//			PrintPre();
//			Serial.println(F("OnAwaitingLink timeout."));
//			//UpdateLinkStage(LinkStageEnum::Disabled);
//			UpdateLinkStage(LinkStageEnum::AwaitingLink);
//		}
//		else if (PartnerAck)
//		{
//			if ((millis() > PreLinkPacketSchedule) && PacketService.CanSendPacket())
//			{
//				TrySendDiscovery();
//			}
//			else
//			{
//				Task::enable();
//			}
//		}
//		else if ((millis() > PreLinkPacketSchedule) && PacketService.CanSendPacket())
//		{
//			TrySendDiscovery();
//		}
//		else
//		{
//			Task::enable();
//		}
//	}
//
//	virtual void OnLinking(const uint32_t stateElapsed) final
//	{
//		if (stateElapsed > LINKING_STAGE_TIMEOUT)
//		{
//			PrintPre();
//			Serial.println(F("Linking timed out!"));
//			UpdateLinkStage(LinkStageEnum::AwaitingLink);
//		}
//		else
//		{
//			Task::enable();
//		}
//	}
//
//private:
//	void TrySendDiscovery()
//	{
//		OutPacket.SetPort(Unlinked::PORT);
//		OutPacket.Payload[SubHeaderDefinition::SUB_HEADER_INDEX] = Unlinked::PartnerDiscovery::SUB_HEADER;
//		OutPacket.Payload[Unlinked::PartnerDiscovery::PAYLOAD_PARTNER_ACKNOWLEDGED_INDEX] = PartnerFound;
//
//		if (this->SendPacket(nullptr, LooseHandle, OutPacket.Content, Unlinked::PartnerDiscovery::PAYLOAD_SIZE))
//		{
//			PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::PartnerDiscovery::PAYLOAD_SIZE);
//			PrintPre();
//			Serial.print(F("Sent Discovery "));
//			if (PartnerFound)
//			{
//				Serial.println(F("(true)"));
//			}
//			else
//			{
//				Serial.println(F("(false)"));
//			}
//			Task::enable();
//		}
//		else
//		{
//			Task::enableDelayed(1);
//		}
//	}
//
//protected:
//	virtual void OnEvent(const PacketEventEnum packetEvent) final
//	{
//		switch (packetEvent)
//		{
//		case PacketEventEnum::Received:
//			ReceivedCount++;
//			break;
//		case PacketEventEnum::ReceivedAck:
//			ReceivedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedAck:
//			break;
//		case PacketEventEnum::ReceiveRejectedCounter:
//			break;
//		case PacketEventEnum::ReceiveRejectedDriver:
//			DroppedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedLink:
//			DroppedCount++;
//			break;
//		case PacketEventEnum::ReceiveRejectedMacCrc:
//			break;
//		case PacketEventEnum::SendAckFailed:
//			break;
//		case PacketEventEnum::SendCollision:
//			break;
//		case PacketEventEnum::Sent:
//			SentCount++;
//			break;
//		case PacketEventEnum::SentAck:
//			SentCount++;
//			break;
//		case PacketEventEnum::SentWithAck:
//			SentCount++;
//			break;
//		default:
//			break;
//		}
//
//		switch (packetEvent)
//		{
//		case PacketEventEnum::Received:
//			break;
//		case PacketEventEnum::ReceivedAck:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceivedAck"));
//			break;
//		case PacketEventEnum::ReceiveRejectedAck:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedAck"));
//			break;
//		case PacketEventEnum::ReceiveRejectedCounter:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedCounter"));
//			break;
//		case PacketEventEnum::ReceiveRejectedDriver:
//			Serial.println(F("@Link Event: ReceiveRejectedDriver"));
//			break;
//		case PacketEventEnum::ReceiveRejectedLink:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejected Link Busy"));
//			break;
//		case PacketEventEnum::ReceiveRejectedMacCrc:
//			PrintPre();
//			Serial.println(F("@Link Event: ReceiveRejectedMacCrc"));
//			break;
//		case PacketEventEnum::SendAckFailed:
//			PrintPre();
//			Serial.println(F("@Link Event: SendAckFailed"));
//			break;
//		case PacketEventEnum::SendCollision:
//			PrintPre();
//			Serial.println(F("@Link Event: SendCollision"));
//			break;
//		case PacketEventEnum::Sent:
//			break;
//		case PacketEventEnum::SentAck:
//			PrintPre();
//			Serial.println(F("@Link Event: SentAck"));
//			break;
//		case PacketEventEnum::SentWithAck:
//			PrintPre();
//			Serial.println(F("@Link Event: SentWithAck"));
//			break;
//		default:
//			break;
//		}
//	}
//};
#endif