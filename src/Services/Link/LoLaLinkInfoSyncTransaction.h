// LoLaLinkInfoSyncTransaction.h

#ifndef _INFOSYNCTRANSACTION_h
#define _INFOSYNCTRANSACTION_h

#include <stdint.h>


class InfoSyncTransaction
{
public:
	enum StageEnum : uint8_t
	{
		StageStart = 0,
		StageHostRTT = 1,
		StageHostRSSI = 2,
		StageRemoteRSSI = 3,
		StagesDone = 4
	};

	enum ContentIdEnum : uint8_t
	{
		ContentHostRTT = (0xFF - 01),
		ContentHostRSSI = (0xFF - 02),
		ContentRemoteRSSI = (0xFF - 03),
	};

protected:
	StageEnum Stage = StageEnum::StageStart;

public:
	virtual bool OnUpdateReceived(const uint8_t contentId) {}
	virtual void OnRequestAckReceived(const uint8_t contentId) {}
	virtual void OnAdvanceRequestReceived(const uint8_t contentId) {}

public:
	void Clear()
	{
		Stage = StageEnum::StageStart;
	}

	StageEnum GetStage()
	{
		return Stage;
	}

	void Advance()
	{
		switch (Stage)
		{
		case StageEnum::StageStart:
			Stage = InfoSyncTransaction::StageEnum::StageHostRTT;
			break;
		case StageEnum::StageHostRTT:
			Stage = InfoSyncTransaction::StageEnum::StageHostRSSI;
			break;
		case StageEnum::StageHostRSSI:
			Stage = InfoSyncTransaction::StageEnum::StageRemoteRSSI;
			break;
		case StageEnum::StageRemoteRSSI:
			Stage = StageEnum::StagesDone;
			break;
		default:
			break;
		}
	}


	bool IsComplete() { return Stage == StageEnum::StagesDone; }
};

class RemoteInfoSyncTransaction : public InfoSyncTransaction
{
public:
	RemoteInfoSyncTransaction() : InfoSyncTransaction()
	{

	}

	bool OnUpdateReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageStart:
			if (contentId == ContentIdEnum::ContentHostRTT)
			{
				Advance();
				return true;
			}
			break;
		case StageEnum::StageHostRSSI:
			if (contentId == ContentIdEnum::ContentHostRSSI)
			{
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	}

	void OnAdvanceRequestReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI)
			{
				Advance();
				return;
			}
			break;
		default:
			break;
		}
	}

	void OnRequestAckReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageHostRTT:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentHostRTT)
			{
				Advance();
				return;
			}
			break;
		case StageEnum::StageHostRSSI:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentHostRSSI)
			{
				Advance();
				return;
			}
			break;
		default:
			break;
		}
	}
};

class HostInfoSyncTransaction : public InfoSyncTransaction
{
public:
	HostInfoSyncTransaction() : InfoSyncTransaction()
	{

	}

	bool OnUpdateReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			if (contentId == ContentIdEnum::ContentRemoteRSSI)
			{
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	}

	void OnRequestAckReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI)
			{
				Advance();
				return;
			}
			break;
		default:
			break;
		}
	}

	void OnAdvanceRequestReceived(const uint8_t contentId)
	{
		switch (Stage)
		{
		case StageEnum::StageHostRTT:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentHostRTT)
			{
				Advance();
				return;
			}
			break;
		case StageEnum::StageHostRSSI:
			if (contentId == InfoSyncTransaction::ContentIdEnum::ContentHostRSSI)
			{
				Advance();
				return;
			}
			break;
		default:
			break;
		}
	}
};

#endif