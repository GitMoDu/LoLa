// TimedStateTransition.h

#ifndef _TIMED_STATE_TRANSITION_h
#define _TIMED_STATE_TRANSITION_h

#include <stdint.h>

template<const uint32_t TransitionTimeout,
	const uint32_t ResendPeriod>
class ServerTimedStateTransition
{
private:
	uint32_t TransitionStart = 0;
	uint32_t LastSent = 0;
	bool TransitionAcknowledged = false;

public:
	ServerTimedStateTransition()
	{}

public:

	void OnReceived()
	{
		TransitionAcknowledged = true;
	}

	void OnSent(const uint32_t timestamp)
	{
		LastSent = timestamp;
	}

public:
	void OnStart(const uint32_t timestamp)
	{
		TransitionAcknowledged = false;
		TransitionStart = timestamp;
		LastSent = timestamp - ResendPeriod;
	}

	void CopyDurationUntilTimeOutTo(const uint32_t timestamp, uint8_t* target)
	{
		const uint32_t remaining = GetDurationUntilTimeOut(timestamp);

		target[0] = remaining;
		target[1] = remaining >> 8;
		target[2] = remaining >> 16;
		target[3] = remaining >> 24;
	}

	const bool IsSendRequested(const uint32_t timestamp)
	{
		return !HasAcknowledge() && (timestamp - LastSent) > ResendPeriod;
	}

	const uint32_t GetDurationUntilTimeOut(const uint32_t timestamp)
	{
		if (timestamp - TransitionStart >= TransitionTimeout)
		{
			return 0;
		}
		else
		{
			return TransitionTimeout - (timestamp - TransitionStart);
		}
	}

	const bool HasTimedOut(const uint32_t timestamp)
	{
		return timestamp - TransitionStart > TransitionTimeout;
	}

	const bool HasAcknowledge()
	{
		return TransitionAcknowledged;
	}
};

template<const uint32_t TransitionTimeout, const uint32_t ResendPeriod>
class ClientTimedStateTransition
{
private:
	enum ClientTransitionStateEnum
	{
		WaitingForTransitionStart,
		TransitionAcknowledged,
		TransitionAcknowledging
	};

private:
	uint32_t TransitionEnd = 0;
	uint32_t LastSent = 0;

	ClientTransitionStateEnum State = ClientTransitionStateEnum::WaitingForTransitionStart;

public:
	ClientTimedStateTransition()
	{}

public:
	void OnReceived(const uint32_t receiveTimestamp, const uint8_t* source)
	{
		uint32_t remaining = source[0];
		remaining += (uint16_t)source[1] << 8;
		remaining += (uint32_t)source[2] << 16;
		remaining += (uint32_t)source[3] << 24;


		State = ClientTransitionStateEnum::TransitionAcknowledging;
		if (remaining < (TransitionTimeout * 2))
		{
			TransitionEnd = receiveTimestamp + remaining;
		}
		else
		{
			TransitionEnd = TransitionTimeout;
		}
		LastSent = receiveTimestamp - ResendPeriod;
	}

	void OnStart()
	{
		State = ClientTransitionStateEnum::WaitingForTransitionStart;
	}

	void OnSent(const uint32_t timestamp)
	{
		LastSent = timestamp;
		State = ClientTransitionStateEnum::TransitionAcknowledged;
	}

public:
	const bool IsSendRequested(const uint32_t timestamp)
	{
		return State == ClientTransitionStateEnum::TransitionAcknowledging && (timestamp - LastSent) > ResendPeriod;
	}

	const bool HasTimedOut(const uint32_t timestamp)
	{
		return ((int32_t)(timestamp - TransitionEnd)) >= 0;
	}

	const uint32_t GetDurationUntilTimeOut(const uint32_t timestamp)
	{
		if (((int32_t)(TransitionEnd - timestamp)) < 0)
		{
			return 0;
		}
		else
		{
			return TransitionEnd - timestamp;
		}
	}

	const bool HasAcknowledge()
	{
		return State != ClientTransitionStateEnum::WaitingForTransitionStart;
	}
};
#endif