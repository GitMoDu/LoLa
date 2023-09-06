// IDuplex.h

#ifndef _I_DUPLEX_h
#define _I_DUPLEX_h

#include <stdint.h>

/// <summary>
/// Duplex interface.
/// </summary>
class IDuplex
{
public:
	static const uint16_t DUPLEX_FULL = 0;

public:
	/// <summary>
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="duration"></param>
	/// <returns>True when the duplex is in transmission range, for the given start and duration.</returns>
	virtual const bool IsInRange(const uint32_t timestamp, const uint16_t duration) { return false; }

	/// <summary>
	/// </summary>
	/// <returns>Usable range in microseconds.</returns>
	virtual const uint16_t GetRange() { return UINT16_MAX; }

	/// <summary>
	/// </summary>
	/// <returns>DUPLEX_FULL if full duplex; Duplex period in microseconds otherwise.</returns>
	virtual const uint16_t GetPeriod() { return DUPLEX_FULL; }
};
#endif