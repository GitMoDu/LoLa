// LoLaLinkSession.h

#ifndef _LOLA_LINK_SESSION_h
#define _LOLA_LINK_SESSION_h

#include <stdint.h>

class LoLaLinkSession
{
protected:
	/// <summary>
	/// Ephemeral session Id.
	/// </summary>
	uint8_t SessionId[LoLaLinkDefinition::SESSION_ID_SIZE];

public:
	virtual void SetSessionId(const uint8_t* sessionId)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
		{
			SessionId[i] = sessionId[i];
		}
	}

	void CopySessionIdTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
		{
			target[i] = SessionId[i];
		}
	}

	const bool SessionIdMatches(const uint8_t* sessionId)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
		{
			if (sessionId[i] != SessionId[i])
			{
				return false;
			}
		}
		return true;
	}
};
#endif