// 
// 
// 

#include <PacketDriver\LoLaPacketDriver.h>

LoLaPacketDriver::LoLaPacketDriver() : ILoLa(), Services()
{
}

void LoLaPacketDriver::OnBatteryAlarm()
{
}

void LoLaPacketDriver::OnSentOk()
{
	if (OutgoingInfo.HasPending())
	{
		LastValidSent = millis();
		if (OutgoingInfo.GetSentHeader() != PACKET_DEFINITION_ACK_HEADER)
		{
			Services.ProcessSent(OutgoingInfo.GetSentHeader());
		}
		OutgoingInfo.Clear();
	}
#ifdef DEBUG_LOLA
	else
	{
		Serial.println(F("Unexpected OnSent event!"));
	}
#endif

	TransmitedCount++;
}

void LoLaPacketDriver::OnWakeUpTimer()
{
}

//When RF detects incoming packet.
void LoLaPacketDriver::OnIncoming(const int16_t rssi)
{
	LastReceived = millis();
	LastReceivedRssi = rssi;
	IncomingInfo.SetInfo(LastReceived, LastReceivedRssi);
}

//When RF has packet to read.
void LoLaPacketDriver::OnReceiveBegin(const uint8_t length, const int16_t rssi)
{
	if (!IncomingInfo.HasInfo())
	{
		IncomingInfo.SetInfo(millis(), rssi);
	}
	Receiver.SetBufferSize(length);
}

//When RF has received a garbled packet.
void LoLaPacketDriver::OnReceivedFail(const int16_t rssi)
{
	IncomingInfo.Clear();
}

void LoLaPacketDriver::OnReceived()
{
	if (!SetupOk || !Enabled || !Receiver.ReceivePacket() || !(Receiver.GetIncomingDefinition() != nullptr))
	{
		RejectedCount++;
	}
	else
	{	//Packet received Ok, let's commit that info really quick.
		LastValidReceived = IncomingInfo.GetPacketTime();
		LastValidReceivedRssi = IncomingInfo.GetPacketRSSI();
		IncomingInfo.Clear();
		ReceivedCount++;

		//Is Ack
		if (Receiver.GetIncomingDefinition()->IsAck())
		{
#ifdef USE_MOCK_PACKET_LOSS
			if (!GetLossChance())
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Packet Lost, Oh noes!"));
#endif
				return;
			}
#endif
			Services.ProcessAck(Receiver.GetIncomingPacket());
		}
		//Is packet
		else
		{
#ifdef USE_MOCK_PACKET_LOSS
			if (!GetLossChance())
			{
#ifdef DEBUG_LOLA
				Serial.println("Packet Lost, Oh noes!");
#endif
				return;
			}
#endif
			if (Receiver.GetIncomingDefinition()->HasACK())
			{
				if (Sender.SendAck(Receiver.GetIncomingDefinition(), Receiver.GetIncomingPacket()->GetId()))
				{
#ifdef USE_MOCK_PACKET_LOSS
					if (!GetLossChance())
					{
#ifdef DEBUG_LOLA
						Serial.println(F("Packet Lost, Oh noes!"));
#endif
						return;
					}
#endif
					if (Transmit())
					{
						LastSent = millis();
						OutgoingInfo.SetPending(PACKET_DEFINITION_ACK_HEADER, Sender.GetBufferSize(), LastSent);
					}
					else
					{
						//Transmit failed.
						//TODO: Store statistics.
					}
				}
			}

			//Handle packet.
			Services.ProcessPacket(Receiver.GetIncomingPacket());
		}

		Receiver.SetBufferSize(0);
	}
}

bool LoLaPacketDriver::Setup()
{
	SetupOk = false;
	if (Receiver.Setup(&PacketMap) &&
		Sender.Setup(&PacketMap))
	{
		IncomingInfo.Clear();
		OutgoingInfo.Clear();

		SetupOk = true;
	}

	return SetupOk;
}

LoLaServicesManager* LoLaPacketDriver::GetServices()
{
	return &Services;
}

bool LoLaPacketDriver::HotAfterSend()
{
	if (LastSent != ILOLA_INVALID_MILLIS && LastValidSent != ILOLA_INVALID_MILLIS)
	{
		if (LastSent - LastValidSent > 0)
		{
			return millis() - LastSent <= LOLA_LINK_SEND_BACK_OFF_DURATION_MILLIS;
		}
		else
		{
			return millis() - LastValidSent <= LOLA_LINK_RE_SEND_BACK_OFF_DURATION_MILLIS;
		}
	}
	else if (LastValidSent != ILOLA_INVALID_MILLIS)
	{
		return millis() - LastValidSent <= LOLA_LINK_RE_SEND_BACK_OFF_DURATION_MILLIS;
	} 
	else if(LastSent != ILOLA_INVALID_MILLIS)
	{
		return millis() - LastSent <= LOLA_LINK_SEND_BACK_OFF_DURATION_MILLIS;
	}

	return false;
}

bool LoLaPacketDriver::HotAfterReceive()
{
	if (LastReceived != ILOLA_INVALID_MILLIS)
	{
		return millis() - LastReceived <= LOLA_LINK_RECEIVE_BACK_OFF_DURATION_MILLIS;

	}
	return false;
}

#ifdef USE_TIME_SLOT
bool LoLaPacketDriver::IsInSendSlot()
{
#ifdef USE_LATENCY_COMPENSATION
	SendSlotElapsed = (GetSyncMillis() - (uint32_t)ETTM) % DuplexPeriodMillis;
#else
	SendSlotElapsed = GetSyncMillis() % DuplexPeriodMillis;
#endif	

	//Even spread of true and false across the DuplexPeriod
	if (EvenSlot)
	{
		if ((SendSlotElapsed < (DuplexPeriodMillis / 2)) &&
			SendSlotElapsed > 0)
		{
			return true;
		}
	}
	else
	{
		if ((SendSlotElapsed > (DuplexPeriodMillis / 2)) &&
			SendSlotElapsed < DuplexPeriodMillis)
		{
			return true;
		}
	}

	return false;
}
#endif

bool LoLaPacketDriver::AllowedSend(const bool overridePermission)
{
	if (!Enabled)
	{
		return false;
	}

#ifdef USE_TIME_SLOT
	if (LinkActive)
	{
		return !OutgoingInfo.HasPending() && CanTransmit() &&
			!HotAfterSend() &&
			(overridePermission || IsInSendSlot());
	}
	else
	{
		return overridePermission && !OutgoingInfo.HasPending() && 
			CanTransmit() && !HotAfterSend() && !HotAfterReceive();
	}
#else
	return Enabled &&
		!HotAfterSend() && !HotAfterReceive() &&
		CanTransmit() &&
		(overridePermission || LinkActive);
#endif
}

bool LoLaPacketDriver::SendPacket(ILoLaPacket* packet)
{
	if (SetupOk)
	{
		if (Sender.SendPacket(packet))
		{
#ifdef USE_MOCK_PACKET_LOSS
			if (!GetLossChance())
			{
#ifdef DEBUG_LOLA
				Serial.println("Packet Lost, Oh noes!");
				return true;
#endif
			}
#endif	
			if (Transmit())
			{
				LastSent = millis();
				OutgoingInfo.SetPending(packet->GetDataHeader(), Sender.GetBufferSize(), LastSent);
				return true;
			}
		
		}
	}

	return false;
}


