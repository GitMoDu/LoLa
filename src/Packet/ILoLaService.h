// ILoLaService.h

#ifndef _ILOLA_SERVICE_h
#define _ILOLA_SERVICE_h

#include <stdint.h>
#include <LoLaDefinitions.h>

class ILoLaService
{
public:
	// Returning false denies Ack response, if packet has Ack.
	virtual bool OnPacketReceived(const uint8_t header, const uint32_t timestamp, uint8_t* payload) { return false; }

	virtual void OnAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

	virtual void OnLinkStatusChanged(const bool linked) {}

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial)
	{
		serial->print(F("No Name"));
	}
#endif
};
#endif