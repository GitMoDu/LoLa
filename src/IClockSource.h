// IClockSource.h

#ifndef _I_CLOCK_SOURCE_h
#define _I_CLOCK_SOURCE_h

class IClockSource
{
public:
	class IClockListener
	{
	public:
		virtual void OnClockTick() {}
	};

public:
	/// <summary>
	/// Adjust clock source to speed up/slow down as requested.
	/// </summary>
	/// <param name="ppm">Absolute tune value, in us/s (ppm).</param>
	virtual void TuneClock(const int16_t ppm) {}

	/// <summary>
	/// Inform clock consumer that clock implementation has no tick callback.
	/// This sets the tick-trained timer correction.
	/// Overflows the raw timer result onto seconds if disabled.
	/// </summary>
	/// <returns>Enable tick-trained timer correction.</returns>
	virtual const bool ClockHasTick() { return false; }

	virtual const uint32_t GetTicklessOverflows() { return 0; }

	/// <summary>
	/// Start the clock source, 
	/// with tune parameter to speed up/slow down as requested.
	/// </summary>
	/// <param name="tickListener">Tick listener interface for callback.</param>
	/// <param name="ppm">Absolute tune value, in us/s.</param>
	/// <returns></returns>
	virtual const bool StartClock(IClockListener* tickListener, const int16_t ppm) { return false; }

	/// <summary>
	/// Stop the clock source.
	/// </summary>
	virtual void StopClock() {}
};
#endif