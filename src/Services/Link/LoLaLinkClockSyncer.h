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
	ILoLaClockSource* SyncedClock = nullptr;

	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = ILOLA_INVALID_MILLIS;

	static const int32_t MAX_TUNE_ERROR_MICROS = 10000;
	//static const int32_t MAX_FINE_TUNE_ERROR_MICROS = 1001;
	static const uint8_t MAX_TUNE_DISCARD_COUNT = 1;

	static const int32_t DRIFT_REFERENCE_PERIOD_MILLIS = 60000;

#ifdef LOLA_DEBUG_CLOCK_SYNC
	bool FirstLine = true;
#endif




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

	bool Setup(ILoLaClockSource* syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	void StampSynced()
	{
		LastSynced = millis();
	}

public:
	virtual void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = ILOLA_INVALID_MILLIS;
		FirstLine = true;
	}
};

class LinkRemoteClockSyncer : public LoLaLinkClockSyncer
{
private:
	enum RemoteSyncEnum : uint8_t
	{
		NoHostSync = 0,
		HasFirstSync = 1,
		NeedsHostTunning = 2,
		HostSynced = 3
	} Status = RemoteSyncEnum::NoHostSync;

	uint32_t LastTuneReceivedMillis = ILOLA_INVALID_MILLIS;

	static const uint8_t TUNE_ERROR_SAMPLES_COUNT = 5;

	ClockErrorStatistics<TUNE_ERROR_SAMPLES_COUNT> TuneErrorStatistics;

	uint8_t TuneDiscard = 0;
	ClockErrorSample SampleGrunt;

private:
	inline void UpdateDrift()
	{
		if (abs(TuneErrorStatistics.GetAverageError()) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
		{
			if (abs(SampleGrunt.ErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
			{
				AddDriftCompensationMicros((int64_t)TuneErrorStatistics.GetAverageError() / ((int64_t)21));
			}
			else
			{
				AddDriftCompensationMicros((int64_t)TuneErrorStatistics.GetAverageError() / ((int64_t)16));
			}
		}
		else
		{
			AddDriftCompensationMicros((int64_t)SampleGrunt.ErrorMicros / ((int64_t)9));
		}
	}

public:
	LinkRemoteClockSyncer() : LoLaLinkClockSyncer() {}

	void Reset()
	{
		Status = RemoteSyncEnum::NoHostSync;

		SyncGoodCount = 0;
		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
		LastSynced = ILOLA_INVALID_MILLIS;
		LastTuneReceivedMillis = ILOLA_INVALID_MILLIS;
		TuneErrorStatistics.Reset();
	}

	bool IsTimeToTune()
	{
		if (LastSynced == ILOLA_INVALID_MILLIS ||
			LastTuneReceivedMillis == ILOLA_INVALID_MILLIS)
		{
			return true;
		}
		else
		{
			switch (Status)
			{
			case RemoteSyncEnum::HasFirstSync:
				return (millis() - LastSynced > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_FAST_PERIOD) &&
					(millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_FAST_PERIOD);
				break;
			case RemoteSyncEnum::NeedsHostTunning:
				if (TuneDiscard > 0)
				{
					return millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_FAST_PERIOD;
				}
				else if (TuneErrorStatistics.HasEnoughSamples())
				{
					if (TuneErrorStatistics.HasFullSamples() &&
						TuneErrorStatistics.GetMaxErrorMicros() < LOLA_LINK_SERVICE_LINKED_CLOCK_GREAT_ERROR_MICROS)
					{
						return millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD;
					}
					else
					{
						return millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_MIN_PERIOD;
					}
				}
				else
				{
					return millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_FAST_PERIOD;
				}
				break;
			case RemoteSyncEnum::HostSynced:
				return true;
			default:
				return true;
			}
		}
	}

	void SetStartingDrift(const int32_t driftErrorMicros)
	{
		SetDriftCompensationMicros(driftErrorMicros);
	}

	void SetSynced()
	{
		Status = RemoteSyncEnum::HasFirstSync;
		StampSynced();
		LastTuneReceivedMillis = millis();
		//LastGreatSync = ILOLA_INVALID_MILLIS;
		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
#ifdef LOLA_DEBUG_CLOCK_SYNC
		FirstLine = true;
#endif
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
		if (LastTuneReceivedMillis == ILOLA_INVALID_MILLIS)
		{
			SampleGrunt.Set(estimationErrorMicros, millis() - LastSynced);
		}
		else
		{
			SampleGrunt.Set(estimationErrorMicros, millis() - LastTuneReceivedMillis);
		}
		LastTuneReceivedMillis = millis();


		//Sample storing and discarding.
		switch (Status)
		{
		case RemoteSyncEnum::NoHostSync:
			Serial.print("NoHostSync");
			break;
		case RemoteSyncEnum::HasFirstSync:
			AddOffsetMicros(estimationErrorMicros);
			if (abs(estimationErrorMicros) < LOLA_LINK_SERVICE_LINKED_SAMPLE_DISCARD_MICROS)
			{
				TuneErrorStatistics.AddTuneSample(SampleGrunt);
				TuneDiscard = 0;
			}
			else if (TuneDiscard < MAX_TUNE_DISCARD_COUNT)
			{
				TuneDiscard++;
			}
			break;
		case RemoteSyncEnum::NeedsHostTunning:
			//Discard offset?
			if (TuneErrorStatistics.HasFullSamples() &&
				abs(TuneErrorStatistics.GetAverageError()) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS &&
				abs(estimationErrorMicros) > LOLA_LINK_SERVICE_LINKED_CLOCK_DISCARD_ERROR_MICROS &&
				TuneDiscard < MAX_TUNE_DISCARD_COUNT)
			{
				TuneDiscard++;
			}
			else
			{
				AddOffsetMicros(estimationErrorMicros);
				if (abs(estimationErrorMicros) < LOLA_LINK_SERVICE_LINKED_SAMPLE_DISCARD_MICROS)
				{
					TuneErrorStatistics.AddTuneSample(SampleGrunt);
					TuneDiscard = 0;
				}
			}
			UpdateDrift();
			break;
		case RemoteSyncEnum::HostSynced:
			break;
		default:
			break;
		}

#ifdef LOLA_DEBUG_CLOCK_SYNC
		static const char Separator = '\t';
		if (FirstLine)
		{
			FirstLine = false;
			Serial.print("Timestamp");
			Serial.print(Separator);
			Serial.print("Clock Error u");
			Serial.print(Separator);
			Serial.print("Error u/s");
			Serial.print(Separator);
			Serial.print("Avg Error m/s");
			Serial.print(Separator);
			Serial.print("Drift");
			Serial.print(Separator);
			Serial.print("Quality");
			Serial.println();
		}

		Serial.print((millis() - LastSynced));
		Serial.print(Separator);
		Serial.print(estimationErrorMicros);
		Serial.print(Separator);
		Serial.print(SampleGrunt.ErrorMicros);
		Serial.print(Separator);
		Serial.print(TuneErrorStatistics.GetAverageError());
		Serial.print(Separator);
		Serial.print(SyncedClock->GetDriftCompensationMicros());
		Serial.print(Separator);
		Serial.print(GetNormalizedClockQuality());
		Serial.println();
#endif



		switch (Status)
		{
			//case RemoteSyncEnum::NoHostSync:
			//	break;
		case RemoteSyncEnum::HasFirstSync:
			if (abs(SampleGrunt.ErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_DISCARD_ERROR_MICROS &&
				TuneErrorStatistics.HasEnoughSamples())
			{
				//Serial.println("Promoted to NeedsHostTunning");
				Status = RemoteSyncEnum::NeedsHostTunning;
			}
			break;
		case RemoteSyncEnum::NeedsHostTunning:
			/*if (TuneErrorStatistics.HasEnoughSamples() &&
				abs(TuneErrorStatistics.GetAverageError()) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
			{
				Status = RemoteSyncEnum::HostSynced;
			}*/
			break;
		case RemoteSyncEnum::HostSynced:
			if (!TuneErrorStatistics.HasEnoughSamples() &&
				abs(TuneErrorStatistics.GetAverageError()) > LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
			{
				Status = RemoteSyncEnum::NeedsHostTunning;
			}
			break;
		default:
			break;
		}

		if (abs(estimationErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
		{
			return true;
		}

		return false;
	}

	uint8_t GetNormalizedClockQuality()
	{
		if (Status != RemoteSyncEnum::HostSynced ||
			TuneErrorStatistics.GetAverageError() > LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
		{
			return 0;
		}
		else
		{
			return TuneErrorStatistics.GetNormalizedClockErrorQuality();
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
		LastSynced = ILOLA_INVALID_MILLIS;

		SyncedClock->SetRandom();
	}

	bool OnEstimationReceived(const int32_t estimationErrorMicros)
	{
		if (IsSynced())
		{
#ifdef LOLA_DEBUG_CLOCK_SYNC
			static const char Separator = '\t';
			if (FirstLine)
			{
				FirstLine = false;
				Serial.print("Timestamp");
				Serial.print(Separator);
				Serial.print("Clock Error u");
				Serial.println();
			}

			Serial.print((millis() - LastSynced));
			Serial.print(Separator);
			Serial.print(estimationErrorMicros);
			Serial.println();
#endif
		}

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