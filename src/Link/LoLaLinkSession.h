// LoLaLinkSession.h

#ifndef _LOLA_LINK_SESSION_h
#define _LOLA_LINK_SESSION_h

#include <stdint.h>
#include "LoLaLinkDefinition.h"

class LoLaLinkSession
{
protected:
	/// <summary>
	/// Ephemeral session Id.
	/// </summary>
	uint8_t SessionId[LoLaLinkDefinition::SESSION_ID_SIZE];

public:
	LoLaLinkSession()
	{}

	virtual void SetSessionId(const uint8_t sessionId[LoLaLinkDefinition::SESSION_ID_SIZE])
	{
		memcpy(SessionId, sessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
	}

	void CopySessionIdTo(uint8_t* target)
	{
		memcpy(target, SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
	}

	const bool SessionIdMatches(const uint8_t sessionId[LoLaLinkDefinition::SESSION_ID_SIZE])
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