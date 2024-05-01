// Si446xRadioFlow.h

#ifndef _SI446X_RADIO_FLOW_h
#define _SI446X_RADIO_FLOW_h

#include "Si446x.h"

using namespace Si446x;

namespace Si446xRadioFlow
{
	static constexpr uint32_t TX_FLOW_TIMEOUT_MICROS = 5000;

	enum class ReadStateEnum
	{
		NoData,
		TryPrepare,
		TryRead
	};

	struct AsyncReadInterruptsStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile ReadStateEnum State = ReadStateEnum::NoData;
		volatile bool Double = false;

		const bool Pending()
		{
			return State != ReadStateEnum::NoData;
		}

		void SetPending(const uint32_t timestamp)
		{
			if (State != ReadStateEnum::NoData)
			{
				Double = true;
			}
			else
			{
				State = ReadStateEnum::TryPrepare;
			}
			Timestamp = timestamp;
		}

		void Clear()
		{
			State = ReadStateEnum::NoData;
			Double = false;
		}

		void RestartFromDouble()
		{
			Double = false;
			State = ReadStateEnum::TryPrepare;
		}

		const uint32_t Elapsed(const uint32_t timestamp)
		{
			return timestamp - Timestamp;
		}
	};

	struct FlowStruct
	{
		uint32_t Timestamp = 0;
		bool Pending = false;

		void SetPending(const uint32_t timestamp)
		{
			Timestamp = timestamp;
			Pending = true;
		}

		void Clear()
		{
			Pending = false;
		}
	};

	struct HopFlowStruct : public FlowStruct
	{
		uint8_t Channel = 0;

		void SetPendingChannel(const uint32_t timestamp, const uint8_t channel)
		{
			SetPending(timestamp);
			Channel = channel;
		}
	};

	struct RadioEventsFlowStruct : public RadioEventsStruct
	{
		uint32_t RxTimestamp = 0;

		void MergeWith(const RadioEventsStruct& source, const uint32_t timestamp)
		{
			const bool hadRx = RxReady;

			RxStart |= source.RxStart;
			RxReady |= source.RxReady;
			RxFail |= source.RxFail;
			TxDone |= source.TxDone;
			VccWarning |= source.VccWarning;
			CalibrationPending |= source.CalibrationPending;
			Error |= source.Error;

			if (!hadRx && RxReady)
			{
				RxTimestamp = timestamp;
			}
		}

		const bool Pending()
		{
			return RxStart
				|| RxReady
				|| RxFail
				|| TxDone
				|| VccWarning
				|| CalibrationPending
				|| Error;
		}

		void Clear()
		{
			RxStart = false;
			RxReady = false;
			RxFail = false;
			TxDone = false;
			VccWarning = false;
			CalibrationPending = false;
			Error = false;
		}
	};

	struct RxFlowStruct
	{
		uint32_t Timestamp = 0;
		uint8_t Size = 0;
		uint8_t RssiLatch = 0;

		void SetPending(const uint32_t startTimestamp, const uint8_t rssiLatch)
		{
			Timestamp = startTimestamp;
			RssiLatch = rssiLatch;
		}

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};
};
#endif