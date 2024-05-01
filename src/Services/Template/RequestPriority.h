// RequestPriority.h

#ifndef _REQUEST_PRIORITY_
#define _REQUEST_PRIORITY_

#include <stdint.h>

/// <summary>
/// Link service Priority for a send request.
/// Unitless value [0;UINT8_MAX].
/// Reference values for use in Link services.
/// Priority scales with DuplexPeriod:
///	 - lower priorities raise the chances of delaying the send request over to the next duplex cycle.
///  - higher priorities increase the chances of the send request starting within the same duplex cycle.
/// RESERVED_FOR_LINK priority should only be used by Link.
/// FAST is the minimum recommended.
/// REGULAR is the recommended default.
/// IRREGULAR is recommended for non-critical requests.
/// </summary>
enum class RequestPriority : uint8_t
{
	/// <summary>
	/// Reserved for Link.
	/// </summary>
	RESERVED_FOR_LINK = 0,

	/// <summary>
	/// Fastest priority, ASAP after RESERVED_FOR_LINK.
	/// Not recomended.
	/// </summary>
	FASTEST = 1,

	/// <summary>
	/// ASAP (after RESERVED_FOR_LINK), with some co-operative margin.
	/// Minimum recommended priority.
	/// Recommended for critical-latency (< 1ms).
	/// </summary>
	FAST = 20,

	/// <summary>
	/// Standard priority for a quick delivery. Should only skip 1 duplex on high congestion.
	/// Recommended for lower-latency (< Duplex Period).
	/// </summary>
	REGULAR = 75,

	/// <summary>
	/// Should skip 1 duplex most of the time, unless there is medium or low congestion.
	/// Recommended higher-latency (> Duplex Period).
	/// </summary>
	IRREGULAR = 130,

	/// <summary>
	/// Should alway skip 1 duplex, unless there is low congestion.
	/// Recommended for lower time-priority.
	/// </summary>
	SLOW = 200,

	/// <summary>
	/// Should always skip 1/2 duplexes, low congestion speeds it up a bit.
	/// Recomended for delay-tolerant requests.
	/// </summary>
	SLOWEST = UINT8_MAX
};
#endif