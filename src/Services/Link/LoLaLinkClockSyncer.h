// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <LoLaClock\ILoLaClockSource.h>
#include <Services\Link\ClockSyncTransaction.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <LoLaClock\ClockErrorStatistics.h>

class LoLaLinkClockSyncer
{
protected:
	ILoLaClockSource * SyncedClock = nullptr;

	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = 0;

	static const int32_t MAX_TUNE_ERROR_MICROS = 10000;
	static const int32_t MAX_FINE_TUNE_ERROR_MICROS = 1001;
	static const uint8_t MAX_TUNE_DISCARD_COUNT = 2;

	static const int32_t DRIFT_REFERENCE_PERIOD_MILLIS = 60000;

	

protected:
	void StampSyncGood()
	{
		if (SyncGoodCount < UINT8_MAX)
		{
			SyncGoodCount++;
		}
	}

	void AddOffsetMicros(const int32_t offset)
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->AddOffsetMicros(offset);
		}
	}

	void SetDriftCompensationMicros(const int32_t drift)
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->SetDriftCompensationMicros(drift);
		}
	}

	void AddDriftCompensationMicros(const int32_t drift)
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->AddDriftCompensationMicros(drift);
		}
	}

public:
	LoLaLinkClockSyncer() {}

	bool Setup(ILoLaClockSource * syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	void StampSynced()
	{
		LastSynced = millis();
	}

public:
	virtual bool IsSynced() { return false; }
	virtual void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = 0;
	}
};

class LinkRemoteClockSyncer : public LoLaLinkClockSyncer
{
private:
	boolean HostSynced = false;
	uint32_t LastTuneReceivedMillis = 0;
	uint32_t LastGreatSync = 0;

	static const uint8_t TUNE_ERROR_SAMPLES_COUNT = 5;

	ClockErrorStatistics<TUNE_ERROR_SAMPLES_COUNT> TuneErrorStatistics;

	uint8_t TuneDiscard = 0;
	ClockErrorSample SampleGrunt;

public:
	LinkRemoteClockSyncer() : LoLaLinkClockSyncer() {}

	bool IsSynced()
	{
		return HasEstimation() && HostSynced;
	}

	void Reset()
	{
		SyncGoodCount = 0;
		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
		LastSynced = 0;
		LastTuneReceivedMillis = 0;
		LastGreatSync = 0;
		HostSynced = false;
		TuneErrorStatistics.Reset();
	}

	bool IsTimeToTune()
	{
		return (LastTuneReceivedMillis == 0 || (millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_MIN_PERIOD))
			&&
			((LastGreatSync == 0) ||
			(LastSynced == 0) ||
			((millis() - LastSynced) > (LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD)));
	}

	void SetStartingDrift(const int32_t driftErrorMicros)
	{
		SetDriftCompensationMicros(driftErrorMicros);
	}

	void SetSynced()
	{
		HostSynced = true;
		StampSynced();
		LastTuneReceivedMillis = millis();
		LastGreatSync = 0;
		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
	}

	bool HasEstimation()
	{
		return SyncGoodCount > 0;
	}

	void OnSyncSecondsReceived(const uint32_t secondsSync)
	{
		SyncedClock->SetSyncSeconds(secondsSync);
	}

	void OnEstimationErrorReceived(const int32_t estimationErrorMicros)
	{
		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		{
			StampSyncGood();
		}

		AddOffsetMicros(estimationErrorMicros);
	}

	/*
		Returns true if clock is ok.
	*/
	bool OnTuneErrorReceived(const int32_t estimationErrorMicros)
	{
		if (LastTuneReceivedMillis == 0)
		{
			LastTuneReceivedMillis = millis() - 1;
		}

		SampleGrunt.PeriodMillis  =	(estimationErrorMicros, millis() - LastTuneReceivedMillis);
		SampleGrunt.ErrorMicros = estimationErrorMicros;
		LastTuneReceivedMillis = millis();

		AddOffsetMicros(SampleGrunt.ErrorMicros);

		if (!TuneErrorStatistics.HasEnoughSamples())
		{
			TuneErrorStatistics.AddTuneSample(SampleGrunt);
			LastGreatSync = 0;
		}
		else if (abs(SampleGrunt.ErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
		{
			LastGreatSync = millis();
			TuneErrorStatistics.AddTuneSample(SampleGrunt);
			AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetWeightedAverageError() / (int64_t)(TuneErrorStatistics.GetWeightedAverageDurationMillis() / 100)));
			TuneDiscard = 0;
			//TODO: Store partner clock drift for future.
		}
		else if (abs(SampleGrunt.ErrorMicros) < MAX_TUNE_ERROR_MICROS &&
			abs(TuneErrorStatistics.GetAverageError()) < MAX_TUNE_ERROR_MICROS)
		{
			TuneErrorStatistics.AddTuneSample(SampleGrunt);			
			AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetWeightedAverageError() / (int64_t)(TuneErrorStatistics.GetWeightedAverageDurationMillis() / 200)));
			LastGreatSync = 0;
		}
		else if (abs(TuneErrorStatistics.GetAverageError()) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS &&
			TuneErrorStatistics.GetWeightedAverageDurationMillis() > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD &&
			TuneDiscard < MAX_TUNE_DISCARD_COUNT)
		{
			TuneDiscard++;
			if (LastGreatSync != 0)
			{
				LastGreatSync = min(millis(), LastGreatSync + SampleGrunt.PeriodMillis);
			}
		}
		else
		{			
			TuneErrorStatistics.AddTuneSample(SampleGrunt);
			AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetAverageError() / (int64_t)(35)));
			LastGreatSync = 0;
		}

#ifdef LOLA_DEBUG_CLOCK_SYNC
		Serial.print("EPeriod: ");
		Serial.println(SampleGrunt.PeriodMillis);
		Serial.print("Error: ");
		Serial.println(SampleGrunt.ErrorMicros);
		Serial.print("Average Error: ");
		Serial.println(TuneErrorStatistics.GetAverageError());
		Serial.print("AWError: ");
		Serial.println(TuneErrorStatistics.GetWeightedAverageError());
		Serial.print("Estimated drift: ");
		Serial.println(SyncedClock->GetDriftCompensationMicros());
		Serial.print("ClockSync Quality: ");
		Serial.println(GetNormalizedClockQuality());
#endif
		if (abs(estimationErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
		{
			StampSyncGood();
			StampSynced();
			
			return true;
		}
		else
		{
			return false;
		}

	}

	uint8_t GetNormalizedClockQuality()
	{
		if(LastGreatSync == 0 ||
			TuneErrorStatistics.GetAverageError() > LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS ||
			TuneErrorStatistics.GetWeightedAverageDurationMillis() < LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD )
		{
			return 0;
		}
		else
		{
			return (map(min(millis() - LastGreatSync, DRIFT_REFERENCE_PERIOD_MILLIS), 0, DRIFT_REFERENCE_PERIOD_MILLIS, 0, UINT8_MAX) +
				TuneErrorStatistics.GetNormalizedClockErrorQuality())
				/ 2;
		}
	}
};

class LinkHostClockSyncer : public LoLaLinkClockSyncer
{
public:
	LinkHostClockSyncer() : LoLaLinkClockSyncer() {}

	bool IsSynced()
	{
		return SyncGoodCount > LOLA_LINK_SERVICE_UNLINK_MIN_CLOCK_SAMPLES;
	}

	void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = 0;

		SyncedClock->SetRandom();
	}

	bool OnEstimationReceived(const int32_t estimationErrorMicros)
	{
		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		{
			StampSyncGood();
			return true;
		}
		else if (!IsSynced())
		{
			//Only reset the sync good count when syncing.
			SyncGoodCount = 0;
		}

		return false;
	}
};

#endif