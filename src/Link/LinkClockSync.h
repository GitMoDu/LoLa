// LinkClockSync.h

#ifndef _LINK_CLOCK_SYNC_h
#define _LINK_CLOCK_SYNC_h

#include <stdint.h>
#include "LoLaLinkDefinition.h"

class LinkClockSync
{
protected:
	enum class ReplyPendingEnum : uint8_t
	{
		None,
		Broad,
		Fine
	};

	enum class ClockSyncStateEnum : uint8_t
	{
		WaitingForStart,
		BroadStarted,
		BroadAccepted,
		FineStarted,
		FineAccepted,
	};

protected:
	ClockSyncStateEnum State = ClockSyncStateEnum::WaitingForStart;
	ReplyPendingEnum ReplyPending = ReplyPendingEnum::None;

public:
	const bool IsBroadAccepted()
	{
		switch (State)
		{
		case ClockSyncStateEnum::BroadAccepted:
		case ClockSyncStateEnum::FineStarted:
		case ClockSyncStateEnum::FineAccepted:
			return true;
			break;
		default:
			return false;
			break;
		}
	}

	const bool IsFineStarted()
	{
		switch (State)
		{
		case ClockSyncStateEnum::FineStarted:
		case ClockSyncStateEnum::FineAccepted:
			return true;
			break;
		default:
			return false;
			break;
		}
	}

	const bool IsFineAccepted()
	{
		switch (State)
		{
		case ClockSyncStateEnum::FineAccepted:
			return true;
			break;
		default:
			return false;
			break;
		}
	}
};

class LinkServerClockSync : public LinkClockSync
{
private:
	int32_t ErrorReply = 0;
	uint8_t GoodCount = 0;

public:
	void Reset()
	{
		State = ClockSyncStateEnum::WaitingForStart;
		ReplyPending = ReplyPendingEnum::None;
		GoodCount = 0;
	}

	void OnReplySent()
	{
		ReplyPending = ReplyPendingEnum::None;
	}

	const bool HasReplyPending()
	{
		return ReplyPending != ReplyPendingEnum::None;
	}

	const bool IsPendingReplyFine()
	{
		return ReplyPending == ReplyPendingEnum::Fine;
	}

	const int32_t GetErrorReply()
	{
		return ErrorReply;
	}

	void OnBroadEstimateReceived(const uint32_t estimate, const uint32_t real)
	{
		switch (State)
		{
		case ClockSyncStateEnum::WaitingForStart:
			State = ClockSyncStateEnum::BroadStarted;
			break;
		case ClockSyncStateEnum::BroadStarted:
		case ClockSyncStateEnum::BroadAccepted:
			break;
		default:
			return;
			break;
		}

		if (estimate >= real)
		{
			ErrorReply = estimate - real;
		}
		else
		{
			ErrorReply = UINT32_MAX - real + estimate;
		}

		ReplyPending = ReplyPendingEnum::Broad;

		static constexpr int32_t ERROR_SECONDS_MAX = 2;

		if (State == ClockSyncStateEnum::BroadStarted
			&& ((ErrorReply >= 0 && ErrorReply < ERROR_SECONDS_MAX)
				|| (ErrorReply < 0 && (-ErrorReply) < ERROR_SECONDS_MAX)))
		{
			State = ClockSyncStateEnum::BroadAccepted;
		}
	}

	void OnFineEstimateReceived(const uint32_t estimate, const uint32_t real)
	{
		switch (State)
		{
		case ClockSyncStateEnum::BroadAccepted:
			State = ClockSyncStateEnum::FineStarted;
			break;
		case ClockSyncStateEnum::FineStarted:
		case ClockSyncStateEnum::FineAccepted:
			break;
		default:
			return;
			break;
		}

		if (estimate >= real)
		{
			ErrorReply = (int32_t)estimate - real;
		}
		else
		{
			ErrorReply = UINT32_MAX - real + estimate;
		}

		ReplyPending = ReplyPendingEnum::Fine;

		if (State == ClockSyncStateEnum::FineStarted
			&& ((ErrorReply >= 0 && (ErrorReply < LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE))
				|| (ErrorReply < 0 && ((-ErrorReply) < LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE))))
		{
			State = ClockSyncStateEnum::FineAccepted;
		}
	}
};

class LinkClientClockSync : public LinkClockSync
{
private:
	uint32_t LastEstimateSent = 0;

public:
	LinkClientClockSync()
	{}

	void Reset(const uint32_t timestamp)
	{
		State = ClockSyncStateEnum::WaitingForStart;
		ReplyPending = ReplyPendingEnum::None;
		LastEstimateSent = timestamp - INT32_MAX;
	}

	const bool IsTimeToSend(const uint32_t timestamp, const uint32_t retryPeriod)
	{
		switch (State)
		{
		case ClockSyncStateEnum::WaitingForStart:
        case ClockSyncStateEnum::BroadAccepted:
			return true;
			break;
		case ClockSyncStateEnum::BroadStarted:
			return !HasPendingReplyBroad() || ((timestamp - LastEstimateSent) >= retryPeriod);
			break;
		case ClockSyncStateEnum::FineStarted:
			return !HasPendingReplyFine() || ((timestamp - LastEstimateSent) >= retryPeriod);
			break;
		default:
			return false;
			break;
		}
	}

	void OnBroadEstimateSent(const uint32_t timestamp)
	{
		switch (State)
		{
		case ClockSyncStateEnum::WaitingForStart:
			State = ClockSyncStateEnum::BroadStarted;
			break;
		case ClockSyncStateEnum::BroadStarted:
			break;
		default:
			return;
			break;
		}

		ReplyPending = ReplyPendingEnum::Broad;
		LastEstimateSent = timestamp;
	}

	void OnFineEstimateSent(const uint32_t timestamp)
	{
		switch (State)
		{
		case ClockSyncStateEnum::BroadAccepted:
			State = ClockSyncStateEnum::FineStarted;
			break;
		case ClockSyncStateEnum::FineStarted:
			break;
		default:
			return;
			break;
		}

		ReplyPending = ReplyPendingEnum::Fine;
		LastEstimateSent = timestamp;
	}

	void OnFineReplyReceived(const uint32_t timestamp, const bool accepted)
	{
		if (State != ClockSyncStateEnum::FineStarted
			|| ReplyPending != ReplyPendingEnum::Fine)
		{
			return;
		}

		ReplyPending = ReplyPendingEnum::None;

		if (accepted)
		{
			State = ClockSyncStateEnum::FineAccepted;
		}
	}

	void OnBroadReplyReceived(const uint32_t timestamp, const bool accepted)
	{
		if (State != ClockSyncStateEnum::BroadStarted
			|| ReplyPending != ReplyPendingEnum::Broad)
		{
			return;
		}

		ReplyPending = ReplyPendingEnum::None;

		if (accepted)
		{
			State = ClockSyncStateEnum::BroadAccepted;
		}
	}

	const bool HasPendingReplyFine()
	{
		return ReplyPending == ReplyPendingEnum::Fine;
	}

	const bool HasPendingReplyBroad()
	{
		return ReplyPending == ReplyPendingEnum::Broad;
	}
};
#endif