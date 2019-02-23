// LoLaPacketDriver.h

#ifndef _LOLAPACKETDRIVER_h
#define _LOLAPACKETDRIVER_h

#include <ILoLa.h>
#include <Packet\LoLaPacketMap.h>

#include <Services\LoLaServicesManager.h>


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

	class OutgoingInfoStruct
	{
	private:
		uint8_t TransmitedHeader = 0;
		uint32_t LastTransmitted = ILOLA_INVALID_MILLIS;
	public:
		bool HasPending()
		{
			return (LastTransmitted != ILOLA_INVALID_MILLIS) && 
				(millis() - LastTransmitted < ILOLA_TRANSMIT_EXPIRY_PERIOD_MILLIS);
		}

		void SetPending(const uint8_t transmitHeader)
		{
			TransmitedHeader = transmitHeader;
			LastTransmitted = millis();
		}

		void SetPending(const uint8_t transmitHeader, const uint32_t timeStampMillis)
		{
			TransmitedHeader = transmitHeader;
			LastTransmitted = timeStampMillis;
		}

		void Clear()
		{
			LastTransmitted = ILOLA_INVALID_MILLIS;
		}

		uint8_t GetSentHeader()
		{
			return TransmitedHeader;
		}
	} OutgoingInfo;


protected:
	///Services that are served receiving packets.
	LoLaServicesManager Services;
	///

	bool SetupOk = false;

public:
	LoLaPacketDriver();
	LoLaServicesManager* GetServices();

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

private:
	inline bool HotAfterSend();
	inline bool HotAfterReceive();

#ifdef USE_TIME_SLOT
	inline bool IsInSendSlot();
#endif

};
#endif