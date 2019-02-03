// LoLaLinkCryptoChallenge.h

#ifndef _LINKCRYPTOCHALLENGE_h
#define _LINKCRYPTOCHALLENGE_h

#include <stdint.h>

class IChallengeTransaction
{
protected:
	uint8_t TransactionId = 0;
	uint32_t Token = 0;
	uint8_t TransactionState = 0;

public:
	virtual bool IsComplete() { return false; }

public:
	virtual void Clear()
	{
		TransactionState = 0;
		TransactionId = 0;
		Token = 0;
	}

	uint8_t GetTransactionId()
	{
		return TransactionId;
	}

	uint32_t GetToken()
	{
		return Token;
	}
};

class ChallengeRequestTransaction : public IChallengeTransaction
{
private:
	enum TransactionStage : uint8_t
	{
		WaitingForReply = 1,
		Complete = 2
	};

private:
	bool IsCryptoTokenReplyAccepted(const uint32_t tokenReply)
	{
		//TODO:
		return Token == tokenReply;
	}

public:
	bool IsComplete()
	{
		return TransactionState == TransactionStage::Complete;
	}

	void NewRequest()
	{
		TransactionId = (uint8_t)random((long)UINT8_MAX);
		Token = (uint32_t)random((long)INT32_MAX); 
		TransactionState = TransactionStage::WaitingForReply;
	}

	void OnReply(const uint8_t transactionId, const uint32_t tokenReply)
	{
		if (TransactionState == TransactionStage::WaitingForReply &&
			TransactionId == transactionId &&
			IsCryptoTokenReplyAccepted(tokenReply))
		{
			TransactionState = TransactionStage::Complete;
		}
		else
		{
			Clear();
		}
	}
};

class ChallengeReplyTransaction : public IChallengeTransaction
{
private:
	enum TransactionStage : uint8_t
	{
		ReplyReady = 1,
		Complete = 2
	};
private:
	uint32_t GetCryptoTokenReply(const uint32_t token)
	{
		//TODO:
		return token;
	}
public:
	bool IsReplyReady()
	{
		return TransactionState == TransactionStage::ReplyReady;
	}

	bool IsComplete()
	{
		return TransactionState == TransactionStage::Complete;
	}

	bool OnReplyAccepted(const uint8_t transactionId)
	{
		if (TransactionId == transactionId)
		{
			TransactionState = TransactionStage::Complete;
		}
		else 
		{
			Clear();
		}

		return IsComplete();
	}

	void OnRequest(const uint8_t transactionId, const uint32_t token)
	{
		Token = token;
		TransactionId = transactionId;
		TransactionState = TransactionStage::ReplyReady;
	}
};
#endif