// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h



#include <ILoLa.h>
#include <Packet\LoLaPacketMap.h>
#include <Transceivers\LoLaReceiver.h>
#include <Transceivers\LoLaSender.h>

#include <Services\LoLaServicesManager.h>

#define LOLA_PACKET_MANAGER_SEND_MIN_BACK_OFF_DURATION_MILLIS 4
#define LOLA_PACKET_MANAGER_SEND_AFTER_RECEIVE_MIN_BACK_OFF_DURATION_MILLIS 4

class LoLaPacketDriver : public ILoLa
{
protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///


	uint32_t LastValidReceived = ILOLA_INVALID_MILLIS;
	int16_t LastValidReceivedRssi = ILOLA_INVALID_RSSI;
	bool SetupOk = false;
	LoLaReceiver Receiver;
	LoLaSender Sender;

public:
	LoLaPacketDriver();
	virtual bool SendPacket(ILoLaPacket* packet);
	LoLaServicesManager* GetServices();
	uint32_t GetLastValidReceivedMillis();
	int16_t GetLastValidRSSI();

public:
	virtual bool Setup();
#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		ILoLa::Debug(serial);
		Services.Debug(serial);
	}
#endif
protected:
	virtual bool Transmit() { return false; }
	virtual bool CanTransmit() { return true; }
	virtual bool AllowedSend();
	
protected:	
	virtual void OnStart() {}

	///Public static members to cross the static events to instance.
public:
	virtual void OnIncoming(const int16_t rssi);
	virtual void OnReceiveBegin(const uint8_t length, const  int16_t rssi);
	virtual void OnReceivedFail(const int16_t rssi);
	virtual void OnSentOk();
	virtual void OnReceived();
	virtual void OnBatteryAlarm();
	virtual void OnWakeUpTimer();
};
#endif

