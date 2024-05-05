// LinkQualityTracker.h

#ifndef _LINK_QUALITY_TRACKER_h
#define _LINK_QUALITY_TRACKER_h

#include "DropRateAccumulator.h"
#include "RssiAccumulator.h"

class LinkQualityTracker
{
private:
	static constexpr uint8_t DROP_REFERENCE = 8;
	static constexpr uint8_t RX_DROP_FILTER_WEIGHT = 220;
	static constexpr uint8_t TX_DROP_FILTER_WEIGHT = 110;

	static constexpr uint8_t RX_RSSI_FILTER_WEIGHT = 200;
	static constexpr uint8_t TX_RSSI_FILTER_WEIGHT = 32;

private:
	const uint32_t TimeoutPeriod;

private:
	uint32_t LastConsume = 0;

	uint32_t LastValidReceived = 0;

protected:
	RxDropAccumulator<RX_DROP_FILTER_WEIGHT, DROP_REFERENCE> RxDropRate{};
	TxDropAccumulator<TX_DROP_FILTER_WEIGHT, DROP_REFERENCE> TxDropRate{};

	RssiAccumulator<RX_RSSI_FILTER_WEIGHT> RxRssi{};
	EmaFilter8<TX_RSSI_FILTER_WEIGHT> TxRssi{};

	uint16_t RxDropCount = 0;

public:
	LinkQualityTracker(const uint32_t timeoutPeriod)
		: TimeoutPeriod(timeoutPeriod)
	{}

	void ResetQuality(const uint32_t timestamp)
	{
		RxDropCount = 0;
		LastConsume = timestamp;
		LastValidReceived = timestamp;
		RxDropRate.Clear();
		TxDropRate.Clear();
		RxRssi.Clear();
		TxRssi.Clear();
	}

	void ResetRssiQuality()
	{
		RxRssi.Clear();
	}

	void OnRxComplete(const uint32_t validTimestamp, const uint8_t rssi, const uint16_t rxLostCount)
	{
		LastValidReceived = validTimestamp;
		RxRssi.Accumulate(rssi);
		RxDropRate.Accumulate(rxLostCount);
		RxDropCount += rxLostCount;
	}

	/// <summary>
	/// Time since a valid packet was received.
	/// </summary>
	/// <returns>Elapsed time in microseconds</returns>
	const uint32_t GetElapsedSinceLastValidReceived()
	{
		return micros() - LastValidReceived;
	}

	/// <summary>
	/// Restart the tracking, assume our last valid packet was just now.
	/// </summary>
	void ResetLastValidReceived(const uint32_t validTimestamp)
	{
		LastValidReceived = validTimestamp;
	}

	/// <summary>
	/// Partner Activity Quality.
	/// Quality is inversely proportional to the elapsed time since last valid received.
	/// </summary>
	/// <returns>Unitless quality [0, UINT8_MAX].</returns>
	const uint8_t GetLastValidReceivedAgeQuality()
	{
		const uint32_t elapsedSinceLastValidReceived = GetElapsedSinceLastValidReceived();

		if (elapsedSinceLastValidReceived > TimeoutPeriod)
		{
			return 0;
		}
		else
		{
			return UINT8_MAX - ((elapsedSinceLastValidReceived * UINT8_MAX) / TimeoutPeriod);
		}
	}

	const uint16_t GetRxDropRate()
	{
		return RxDropRate.GetDropRate();
	}

	const uint16_t GetRxDropCount()
	{
		return RxDropCount;
	}

	const uint16_t GetTxDropRate()
	{
		return TxDropRate.GetDropRate();
	}

	const uint16_t GetRxLoopingDropCount()
	{
		return RxDropRate.GetLoopingDropCount();
	}

	/// <summary>
	/// </summary>
	/// <returns>Unitless quality [0, UINT8_MAX].</returns>
	const uint8_t GetRxDropQuality()
	{
		return RxDropRate.GetQuality();
	}

	/// <summary>
	/// </summary>
	/// <returns>Unitless quality [0, UINT8_MAX].</returns>
	const uint8_t GetTxDropQuality()
	{
		return TxDropRate.GetQuality();
	}

	/// <summary>
	/// RSSI (Received Signal Strength Indicator) Quality.
	/// </summary>
	/// <returns>Unitless quality [0, UINT8_MAX].</returns>
	const uint8_t GetRxRssiQuality()
	{
		return RxRssi.GetQuality();
	}

	/// <summary>
	/// RSSI (Received Signal Strength Indicator) Quality,
	/// as reported by the partner.
	/// </summary>
	/// <returns>Unitless quality [0, UINT8_MAX].</returns>
	const uint8_t GetTxRssiQuality()
	{
		return TxRssi.Get();
	}

protected:
	void CheckUpdateQuality(const uint32_t timestamp)
	{
		// Consume accumulator around every LoLaLinkDefinition::REPORT_UPDATE_PERIOD.
		if (timestamp - LastConsume >= LoLaLinkDefinition::REPORT_UPDATE_PERIOD_MICROS)
		{
			const uint32_t elapsed = timestamp - LastConsume;
			LastConsume = timestamp;

			RxDropRate.Consume(elapsed);
			TxDropRate.Consume(elapsed);
			RxRssi.Consume();
		}
	}
};
#endif