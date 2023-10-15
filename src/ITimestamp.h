// ITimestamp.h
#ifndef _I_TIMESTAMP_h
#define _I_TIMESTAMP_h

#include <stdint.h>

/// <summary>
/// Non-monotonic time stamp.
/// </summary>
class IRollingTimestamp
{
public:
	/// <summary>
	/// </summary>
	/// <returns>Rolling us.</returns>
	virtual const uint32_t GetRollingTimestamp() { return 0; }
};

/// <summary>
/// Monotonic time stamp.
/// </summary>
class IPrecisionTimestamp
{
public:
	virtual const uint32_t GetPrecisionCycles() { return 0; }

	virtual const uint32_t GetPrecisionElapsed(const uint32_t cyclesStart, const uint32_t cyclesEnd) { return 0; }

	virtual const uint32_t GetPrecisionMicros() { return 0; }
};
#endif