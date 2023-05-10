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
	/// <summary>
	/// </summary>
	/// <param name="timestamp"></param>
	/// <returns>True when the duplex is in transmission slot, for the given timestamp</returns>
	virtual const bool IsInSlot(const uint32_t timestamp) { return false; }


	/// <summary>
	/// </summary>
	/// <param name="timestamp"></param>
	/// <returns>True when there is no duplex, helps to optimize linking.</returns>
	virtual const bool IsFullDuplex() { return false; }
};
#endif