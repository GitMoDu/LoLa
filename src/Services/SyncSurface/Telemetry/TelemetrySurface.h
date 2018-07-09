// TelemetrySurface.h

#ifndef _TELEMETRYSURFACE_h
#define _TELEMETRYSURFACE_h

#include <BitTracker.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define TELEMETRY_BLOCK_COUNT 4
#define TELEMETRY_DATA_SIZE (TELEMETRY_BLOCK_COUNT*SYNC_SURFACE_BLOCK_SIZE)

//Base telemetry block map, 4 8bit values for status codes.
/////////////////
#define SYNC_TELEMETRY_HEALTH_STATUS_BLOCK_INDEX	0
/////////////////
#define SYNC_TELEMETRY_LINK_STATUS_BLOCK_INDEX		1
/////////////////
#define SYNC_TELEMETRY_BATTERY_STATUS_BLOCK_INDEX	2
/////////////////
// //TODO:
/////////////////
// //TODO:
/////////////////

#define PACKET_DEFINITION_SYNC_TELEMETRY_BASE_HEADER (PACKET_DEFINITION_SYNC_COMMAND_INPUT_BASE_HEADER+SYNC_SERVICE_PACKET_DEFINITION_COUNT)


#define SYNC_STATUS_BLOCK_INDEX 0

class TelemetrySurface : public TemplateTrackedSurface<TELEMETRY_BLOCK_COUNT>
{
public:
	TelemetrySurface()
		: TemplateTrackedSurface()
	{
	}

	//index [0:3]
	uint8_t GetHealthStatus(const uint8_t index)
	{
		return Get8(SYNC_TELEMETRY_HEALTH_STATUS_BLOCK_INDEX, index);
	}

	//index [0:3]
	uint8_t GetLinkStatus(const uint8_t index)
	{
		return Get8(SYNC_TELEMETRY_HEALTH_STATUS_BLOCK_INDEX, index);
	}

	//index [0:3]
	uint8_t GetBatteryStatus(const uint8_t index)
	{
		return Get8(SYNC_TELEMETRY_HEALTH_STATUS_BLOCK_INDEX, index);
	}

	//index [0:3]
	void SetHealthStatus(const uint8_t statusCode, const uint8_t index)
	{
		Set8(SYNC_TELEMETRY_HEALTH_STATUS_BLOCK_INDEX, statusCode, index);
	}

	//index [0:3]
	void SetLinkStatus(const uint8_t statusCode, const uint8_t index)
	{
		Set8(SYNC_TELEMETRY_LINK_STATUS_BLOCK_INDEX, statusCode, index);
	}

	//index [0:3]
	void SetBatteryStatus(const uint8_t statusCode, const uint8_t index)
	{
		Set8(SYNC_TELEMETRY_BATTERY_STATUS_BLOCK_INDEX, statusCode, index);
	}

#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("Telemetry Surface "));
		ITrackedSurface::Debug(serial);
	}
#endif 
};
#endif

