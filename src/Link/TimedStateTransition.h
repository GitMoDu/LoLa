// TimedStateTransition.h

#ifndef _TIMED_STATE_TRANSITION_h
#define _TIMED_STATE_TRANSITION_h

#include <stdint.h>

class ServerTimedStateTransition
{
private:
	const uint32_t TransitionTimeout;

private:
	uint32_t TransitionStart = 0;
	bool TransitionAcknowledged = false;

public:
	ServerTimedStateTransition(const uint32_t transitionTimeout)
		: TransitionTimeout(transitionTimeout)
	{}

public:
	void OnReceived()
	{
		TransitionAcknowledged = true;
	}

public:
	void OnStart(const uint32_t timestamp)
	{
		TransitionAcknowledged = false;
		TransitionStart = timestamp;
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
		const uint_fast16_t remaining = GetDurationUntilTimeOut(timestamp);

		return remaining != 0
			&& remaining < TransitionTimeout
			&& !HasAcknowledge();
	}

	const uint_fast16_t GetDurationUntilTimeOut(const uint32_t timestamp)
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
		return GetDurationUntilTimeOut(timestamp) == 0;
	}

	const bool HasAcknowledge()
	{
		return TransitionAcknowledged;
	}
};

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
	const uint32_t TransitionTimeout;

private:
	uint32_t TransitionEnd = 0;

	ClientTransitionStateEnum State = ClientTransitionStateEnum::WaitingForTransitionStart;

public:
	ClientTimedStateTransition(const uint32_t transitionTimeout)
		: TransitionTimeout(transitionTimeout)
	{}

public:
	void OnReceived(const uint32_t receiveTimestamp, const uint8_t* source)
	{
		uint32_t remaining = source[0];
		remaining |= (uint32_t)source[1] << 8;
		remaining |= (uint32_t)source[2] << 16;
		remaining |= (uint32_t)source[3] << 24;

		if ((remaining < TransitionTimeout)
			|| (receiveTimestamp + remaining >= TransitionEnd))
		{
			State = ClientTransitionStateEnum::TransitionAcknowledging;
			TransitionEnd = receiveTimestamp + remaining;
		}
#if defined(DEBUG_LOLA)
		else
		{
			Serial.print(F("Rejected transition duration: "));
			Serial.print(remaining);
			Serial.println(F("us"));
		}
#endif
	}

	void Clear()
	{
		State = ClientTransitionStateEnum::WaitingForTransitionStart;
	}

	void OnSent()
	{
		State = ClientTransitionStateEnum::TransitionAcknowledged;
	}

public:
	const bool IsSendRequested(const uint32_t timestamp)
	{
		return State == ClientTransitionStateEnum::TransitionAcknowledging;
	}

	const bool HasTimedOut(const uint32_t timestamp)
	{
		return State != ClientTransitionStateEnum::WaitingForTransitionStart
			&& (TransitionEnd - timestamp >= TransitionTimeout);
	}

	const bool HasAcknowledge()
	{
		return State == ClientTransitionStateEnum::TransitionAcknowledged || State == ClientTransitionStateEnum::TransitionAcknowledging;
	}

#if defined(DEBUG_LOLA_LINK)
	void Debug(const uint32_t timestamp)
	{
		Serial.print(F("State"));
		Serial.print(State);
		Serial.print(F(", TransitionEnd"));
		Serial.print(TransitionEnd);
		Serial.print(F(", timestamp"));
		Serial.print(timestamp);
		Serial.print(F(", delta"));
		Serial.print((int32_t)(TransitionEnd - timestamp));
		Serial.println();
	}
#endif

#if defined(DEBUG_LOLA_LINK)
	const uint32_t GetDurationUntilTimeOut(const uint32_t timestamp)
	{
		if (TransitionEnd - timestamp >= TransitionTimeout)
		{
			return 0;
		}
		else
		{
			return TransitionEnd - timestamp;
		}
	}
#endif
};
#endif