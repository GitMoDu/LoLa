// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLa.h>
#include <Packet\LoLaPacketMap.h>
#include <Transceivers\LoLaReceiver.h>
#include <Transceivers\LoLaSender.h>

#include <Services\LoLaServicesManager.h>

#define LOLA_PACKET_MANAGER_SEND_MIN_BACK_OFF_DURATION_MILLIS 3
#define LOLA_PACKET_MANAGER_SEND_AFTER_RECEIVE_MIN_BACK_OFF_DURATION_MILLIS 5

class LoLaPacketDriver : public ILoLa
{
private:
	class IncomingInfoStruct
	{
	private:
		uint32_t PacketTime = ILOLA_INVALID_MILLIS;
		int16_t PacketRSSI = ILOLA_INVALID_RSSI;

	public:
		IncomingInfoStruct() {}

		uint32_t GetPacketTime()
		{
			return PacketTime;
		}

		int16_t GetPacketRSSI()
		{
			return PacketRSSI;
		}

		void Clear()
		{
			PacketTime = ILOLA_INVALID_MILLIS;
			PacketRSSI = ILOLA_INVALID_RSSI;
		}

		bool HasInfo()
		{
			return PacketTime != ILOLA_INVALID_MILLIS && PacketRSSI != ILOLA_INVALID_RSSI;
		}

		void SetInfo(const uint32_t time, const int16_t rssi)
		{
			PacketTime = time;
			PacketRSSI = rssi;
		}
	} IncomingInfo;

	//Helper for IsInSendSlot().
	uint32_t SendSlotElapsed;

	///Link Info for use of estimated latency.
	LoLaLinkInfo* LinkInfo = nullptr;
	///

protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///

	bool SetupOk = false;
	LoLaReceiver Receiver;
	LoLaSender Sender;

public:
	LoLaPacketDriver();
	LoLaServicesManager* GetServices();
	void SetCryptoSeedSource(ISeedSource* cryptoSeedSource);

public:
	virtual bool SendPacket(ILoLaPacket* packet);

public:
	virtual bool Setup();
	virtual void OnIncoming(const int16_t rssi);
	virtual void OnReceiveBegin(const uint8_t length, const int16_t rssi);
	virtual void OnReceivedFail(const int16_t rssi);
	virtual void OnSentOk();
	virtual void OnReceived();
	virtual void OnBatteryAlarm();
	virtual void OnWakeUpTimer();
	virtual bool AllowedSend(const bool overridePermission = false);

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

protected:
	virtual void OnStart() {}

private:
	inline bool HotAfterSend();
	inline bool HotAfterReceive();

	inline bool IsInSendSlot();

};
#endif