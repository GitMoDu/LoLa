// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <LoLaClock\LoLaClock.h>
#include <Link\ClockSyncTransaction.h>
#include <Link\LoLaLinkDefinitions.h>

#include <LoLaClock\ClockErrorStatistics.h>

class LoLaLinkClockSyncer
{
public:
	const static uint8_t LOLA_LINK_CLOCKSYNC_FULL_CLOCK_REQUEST = 0x10;
	const static uint8_t LOLA_LINK_CLOCKSYNC_FULL_CLOCK_RESPONSE = 0x11;
	const static uint8_t LOLA_LINK_CLOCKSYNC_ESTIMATION_REQUEST = 0x20;
	const static uint8_t LOLA_LINK_CLOCKSYNC_ESTIMATION_RESPONSE = 0x21;

	const static uint8_t BROAD_SYNC_RESPONSE = 0xF1;
	const static uint8_t FINE_SYNC_RESPONSE = 0xF2;

	const static uint8_t BROAD_SYNC_REQUEST = 0xF1;
	const static uint8_t FINE_SYNC_REQUEST = 0xF2;

protected:
	ISyncedClock* SyncedClock = nullptr;


	enum ClockSyncerState
	{
		NoSync,
		LinkSync,
		ContinuousTunning
	} State = ClockSyncerState::NoSync;

	//uint8_t SyncGoodCount = 0;
	//uint32_t LastSynced = 0;

	//static const int32_t MAX_TUNE_ERROR_MICROS = 10000;
	//static const int32_t MAX_FINE_TUNE_ERROR_MICROS = 1001;
	//static const uint8_t MAX_TUNE_DISCARD_COUNT = 2;

	//static const int32_t DRIFT_REFERENCE_PERIOD_MILLIS = 60000;

	union ArrayToUint32 {
		uint8_t array[4];
		uint32_t uint;
		int32_t iint;
	};

	const static uint8_t NoResponse = 0;
	uint8_t PendingResponse = 0;

public:
	LoLaLinkClockSyncer(ISyncedClock* syncedClock)
		: SyncedClock(syncedClock)
	{
	}

	void Clear()
	{
		State = ClockSyncerState::NoSync;
		ClearResponse();
	}

	void StampSynced()
	{
		State = ClockSyncerState::ContinuousTunning;
	}

	bool HasSync()
	{
		return State == ClockSyncerState::LinkSync || State == ClockSyncerState::ContinuousTunning;
	}

	bool HasResponse()
	{
		return PendingResponse == NoResponse;
	}

	void ClearResponse()
	{
		PendingResponse = NoResponse;
	}

	// Returns sub-header.
	virtual bool OnReceivedLinked(const uint32_t timestamp, uint8_t* payload) { return false; }
	virtual bool OnReceivedLinking(const uint32_t timestamp, uint8_t* payload) { return false; }
	virtual uint8_t GetResponse(uint8_t* outPayload) { return 0; }

protected:
	//void StampSyncGood()
	//{
	//	if (SyncGoodCount < UINT8_MAX)
	//	{
	//		SyncGoodCount++;
	//	}
	//}

	/*void AddOffsetMicros(const int32_t offset)
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
	}*/

	/*void StampSynced()
	{
		LastSynced = millis();
	}

public:
	virtual bool IsSynced() { return false; }
	virtual void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = 0;
	}*/
};

class LinkRemoteClockSyncer : public LoLaLinkClockSyncer
{
	//private:
	//	boolean HostSynced = false;
	//	uint32_t LastTuneReceivedMillis = 0;
	//	uint32_t LastGreatSync = 0;
	//
	//	static const uint8_t TUNE_ERROR_SAMPLES_COUNT = 5;
	//
	//	ClockErrorStatistics<TUNE_ERROR_SAMPLES_COUNT> TuneErrorStatistics;
	//
	//	uint8_t TuneDiscard = 0;
	//	ClockErrorSample SampleGrunt;
	//
public:
	LinkRemoteClockSyncer(ISyncedClock* syncedClock) : LoLaLinkClockSyncer(syncedClock) {}

	void OnReceivedBroadSync()
	{
		TransmittedTimestamp = SyncedClock->GetSyncSeconds();
	}

	//bool Update()
	//{
	//	return false;
	//}
	//
	//	bool IsSynced()
	//	{
	//		return HasEstimation() && HostSynced;
	//	}
	//
	//	void Reset()
	//	{
	//		SyncGoodCount = 0;
	//		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
	//		LastSynced = 0;
	//		LastTuneReceivedMillis = 0;
	//		LastGreatSync = 0;
	//		HostSynced = false;
	//		TuneErrorStatistics.Reset();
	//	}
	//
	//	bool IsTimeToTune()
	//	{
	//		return (LastTuneReceivedMillis == 0 || (millis() - LastTuneReceivedMillis > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_MIN_PERIOD))
	//			&&
	//			((LastGreatSync == 0) ||
	//			(LastSynced == 0) ||
	//				((millis() - LastSynced) > (LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD)));
	//	}
	//
	//	//void SetStartingDrift(const int32_t driftErrorMicros)
	//	//{
	//	//	SetDriftCompensationMicros(driftErrorMicros);
	//	//}
	//
	//	void SetSynced()
	//	{
	//		HostSynced = true;
	//		StampSynced();
	//		LastTuneReceivedMillis = millis();
	//		LastGreatSync = 0;
	//		TuneDiscard = MAX_TUNE_DISCARD_COUNT;
	//	}
	//
	//	bool HasEstimation()
	//	{
	//		return SyncGoodCount > 0;
	//	}
	//
	//	void OnSyncSecondsReceived(const uint32_t secondsSync)
	//	{
	//		SyncedClock->SetOffsetSeconds(secondsSync);
	//	}
	//
	//	void OnEstimationErrorReceived(const int32_t estimationErrorMicros)
	//	{
	//		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
	//		{
	//			StampSyncGood();
	//		}
	//
	//		SyncedClock->AddOffsetMicros(estimationErrorMicros);
	//	}
	//
	//	/*
	//		Returns true if clock is ok.
	//	*/
	//	bool OnTuneErrorReceived(const int32_t estimationErrorMicros)
	//	{
	//		if (LastTuneReceivedMillis == 0)
	//		{
	//			LastTuneReceivedMillis = millis() - 1;
	//		}
	//
	//		SampleGrunt.PeriodMillis = (estimationErrorMicros, millis() - LastTuneReceivedMillis);
	//		SampleGrunt.ErrorMicros = estimationErrorMicros;
	//		LastTuneReceivedMillis = millis();
	//
	//		SyncedClock->AddOffsetMicros(SampleGrunt.ErrorMicros);
	//
	//		if (!TuneErrorStatistics.HasEnoughSamples())
	//		{
	//			TuneErrorStatistics.AddTuneSample(SampleGrunt);
	//			LastGreatSync = 0;
	//		}
	//		else if (abs(SampleGrunt.ErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
	//		{
	//			LastGreatSync = millis();
	//			TuneErrorStatistics.AddTuneSample(SampleGrunt);
	//			//AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetWeightedAverageError() / (int64_t)(TuneErrorStatistics.GetWeightedAverageDurationMillis() / 100)));
	//			TuneDiscard = 0;
	//			//TODO: Store partner clock drift for future.
	//		}
	//		else if (abs(SampleGrunt.ErrorMicros) < MAX_TUNE_ERROR_MICROS &&
	//			abs(TuneErrorStatistics.GetAverageError()) < MAX_TUNE_ERROR_MICROS)
	//		{
	//			TuneErrorStatistics.AddTuneSample(SampleGrunt);
	//			//AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetWeightedAverageError() / (int64_t)(TuneErrorStatistics.GetWeightedAverageDurationMillis() / 200)));
	//			LastGreatSync = 0;
	//		}
	//		else if (abs(TuneErrorStatistics.GetAverageError()) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS &&
	//			TuneErrorStatistics.GetWeightedAverageDurationMillis() > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD&&
	//			TuneDiscard < MAX_TUNE_DISCARD_COUNT)
	//		{
	//			TuneDiscard++;
	//			if (LastGreatSync != 0)
	//			{
	//				LastGreatSync = min(millis(), LastGreatSync + SampleGrunt.PeriodMillis);
	//			}
	//		}
	//		else
	//		{
	//			TuneErrorStatistics.AddTuneSample(SampleGrunt);
	//			//AddDriftCompensationMicros((int32_t)((int64_t)TuneErrorStatistics.GetAverageError() / (int64_t)(35)));
	//			LastGreatSync = 0;
	//		}
	//
	//#ifdef LOLA_DEBUG_CLOCK_SYNC
	//		Serial.print("EPeriod: ");
	//		Serial.println(SampleGrunt.PeriodMillis);
	//		Serial.print("Error: ");
	//		Serial.println(SampleGrunt.ErrorMicros);
	//		Serial.print("Average Error: ");
	//		Serial.println(TuneErrorStatistics.GetAverageError());
	//		Serial.print("AWError: ");
	//		Serial.println(TuneErrorStatistics.GetWeightedAverageError());
	//		Serial.print("Estimated drift: ");
	//		Serial.println(SyncedClock->GetDriftCompensationMicros());
	//		Serial.print("ClockSync Quality: ");
	//		Serial.println(GetNormalizedClockQuality());
	//#endif
	//		if (abs(estimationErrorMicros) < LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS)
	//		{
	//			StampSyncGood();
	//			StampSynced();
	//
	//			return true;
	//		}
	//		else
	//		{
	//			return false;
	//		}
	//
	//	}
	//
	//	uint8_t GetNormalizedClockQuality()
	//	{
	//		if (LastGreatSync == 0 ||
	//			TuneErrorStatistics.GetAverageError() > LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS ||
	//			TuneErrorStatistics.GetWeightedAverageDurationMillis() < LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD)
	//		{
	//			return 0;
	//		}
	//		else
	//		{
	//			return (map(min(millis() - LastGreatSync, DRIFT_REFERENCE_PERIOD_MILLIS), 0, DRIFT_REFERENCE_PERIOD_MILLIS, 0, UINT8_MAX) +
	//				TuneErrorStatistics.GetNormalizedClockErrorQuality())
	//				/ 2;
	//		}
	//	}
};

class LinkHostClockSyncer : public LoLaLinkClockSyncer
{
private:
	int32_t EstimationError = 0;
	uint32_t TransmittedTimestamp = 0;

public:
	LinkHostClockSyncer(ISyncedClock* syncedClock) : LoLaLinkClockSyncer(syncedClock) {}

	const uint32_t GetBroadSyncPreSend()
	{
		return SyncedClock->GetSyncSeconds();
	}

	void OnPreSendFineSync()
	{
		TransmittedTimestamp = SyncedClock->GetSyncMicros();
	}

	void OnReceivedBroadSyncAttempt(const uint32_t timestampMicros, const uint32_t remoteTimestamp)
	{
		// Adjusted Time = Current time - Elapsed since packet received .
		// EstimationError = Adjusted - Remote time  -> converted to seconds.
		EstimationError = SyncedClock->GetSyncSeconds() -
			(((micros() - timestampMicros)) / IClock::ONE_SECOND_MICROS) -
			remoteTimestamp;

		PendingResponse = BROAD_SYNC_RESPONSE;
	}

	void OnReceivedFineSyncAttempt(const uint32_t timestampMicros, const uint32_t remoteTimestamp)
	{
		// Adjusted Time = Current time - Elapsed since packet received .
		// EstimationError = Adjusted - Remote time.
		EstimationError = SyncedClock->GetSyncMicros() -
			((micros() - timestampMicros)) -
			remoteTimestamp;

		PendingResponse = BROAD_SYNC_RESPONSE;
	}

	virtual bool OnReceived(const uint8_t subHeader, const uint32_t timestamp, uint8_t* payload)
	{
		ArrayToUint32 ATUI;

		switch (subHeader)
		{
		case LOLA_LINK_CLOCKSYNC_FULL_CLOCK_REQUEST:
			PendingResponse = LOLA_LINK_CLOCKSYNC_FULL_CLOCK_RESPONSE;
			return true;
		case LOLA_LINK_CLOCKSYNC_ESTIMATION_REQUEST:
			for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			{
				ATUI.array[i] = payload[i];
			}
			EstimationError = ATUI.uint - ((SyncedClock->GetSyncMicros() - (micros() - timestamp)));
			PendingResponse = LOLA_LINK_CLOCKSYNC_ESTIMATION_RESPONSE;
			return true;
		default:
			break;
		}

		return false;

		//if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		//{
		//	StampSyncGood();
		//	return true;
		//}
		//else if (!IsSynced())
		//{
		//	//Only reset the sync good count when syncing.
		//	SyncGoodCount = 0;
		//}

		return false;
	}


	// Returns sub-header.
	const uint8_t GetResponse(uint8_t* outPayload)
	{
		ArrayToUint32 ATUI;

		switch (PendingResponse)
		{
		case BROAD_SYNC_RESPONSE:
			ATUI.uint = EstimationError;
			outPayload[0] = ATUI.array[0];
			outPayload[1] = ATUI.array[1];
			outPayload[2] = ATUI.array[2];
			outPayload[3] = ATUI.array[3];
			break;
		case FINE_SYNC_RESPONSE:
			ATUI.uint = EstimationError;
			outPayload[0] = ATUI.array[0];
			outPayload[1] = ATUI.array[1];
			outPayload[2] = ATUI.array[2];
			outPayload[3] = ATUI.array[3];
			break;
			//case LOLA_LINK_CLOCKSYNC_FULL_CLOCK_RESPONSE:
			//	ATUI.uint = SyncedClock->GetSyncSeconds();

			//	for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			//	{
			//		outPayload[i] = ATUI.array[i];
			//	}

			//	ATUI.uint = SyncedClock->GetSyncMicros();

			//	for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			//	{
			//		outPayload[i + sizeof(uint32_t)] = ATUI.array[i];
			//	}
			//	break;
			//case LOLA_LINK_CLOCKSYNC_ESTIMATION_RESPONSE:
			//	break;
		default:
			break;
		}

		return PendingResponse;
	}


	/*

	void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = 0;

		SyncedClock->SetOffsetSeconds(random(0, UINT32_MAX));
	}*/



};

#endif