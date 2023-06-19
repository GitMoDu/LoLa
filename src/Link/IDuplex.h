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
	static const uint32_t DUPLEX_FULL = 0;

public:
	/// <summary>
	/// 
	/// </summary>
	/// <param name="startTimestamp"></param>
	/// <param name="endTimestamp"></param>
	/// <returns>True when the duplex is in transmission range, for the given start end duration.</returns>
	virtual const bool IsInRange(const uint32_t startTimestamp, const uint32_t endTimestamp) { return false; }

	/// <summary>
	/// </summary>
	/// <returns>DUPLEX_FULL if full duplex; usable range in microseconds otherwise.</returns>
	virtual const uint32_t GetRange() { return DUPLEX_FULL; }

	/// <summary>
	/// </summary>
	/// <param name="timestamp"></param>
	/// <returns>True when there is no duplex, helps to optimize linking.</returns>
	//virtual const bool IsFullDuplex() { return false; }
};
#endif