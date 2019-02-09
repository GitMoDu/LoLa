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
		StagesDone = 5
	} Stage = StageEnum::StageStart;

	enum ContentIdEnum : uint8_t
	{
		ContentHostRTT = 0,
		ContentHostRSSI = 1,
		ContentRemoteRSSI = 2
	};

public:
	virtual bool OnUpdateReceived(const uint8_t id) {}
	virtual void OnRequestAckReceived() {}
	virtual void OnAdvanceRequestReceived() {}

public:
	void Clear()
	{
		Stage = StageEnum::StageStart;
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

	bool OnUpdateReceived(const uint8_t id)
	{
		switch (Stage)
		{
		case StageEnum::StageStart:
			if (id == ContentIdEnum::ContentHostRTT)
			{
				Advance();
				return true;
			}
			break;
		case StageEnum::StageHostRTT:
			if (id == ContentIdEnum::ContentHostRSSI)
			{
				Advance();
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	}

	void OnAdvanceRequestReceived()
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			Advance();
			break;
		default:
			break;
		}
	}

	void OnRequestAckReceived()
	{
		switch (Stage)
		{
		case StageEnum::StageHostRTT:			
			Advance();
			break;
		case StageEnum::StageHostRSSI:
			Advance();
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

	bool OnUpdateReceived(const uint8_t id)
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			if (id == ContentIdEnum::ContentRemoteRSSI)
			{
				Advance();
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	}

	void OnRequestAckReceived()
	{
		switch (Stage)
		{
		case StageEnum::StageRemoteRSSI:
			Advance();
			break;
		default:
			break;
		}
	}

	void OnAdvanceRequestReceived()
	{
		switch (Stage)
		{
		case StageEnum::StageHostRTT:
			Advance();
			break;
		case StageEnum::StageHostRSSI:
			Advance();
			break;
		default:
			break;
		}
	}
};

#endif