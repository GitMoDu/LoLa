
#define SERIAL_BAUD_RATE 115200

#define DEBUG_LOLA
#define LOLA_UNIT_TESTING


//#define LOLA_USE_POLY1305

#include <ILoLaInclude.h>
#include <Arduino.h>
#include <Crypto/LoLaCryptoAmSession.h>

static constexpr uint8_t AccessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE] = { 0x10, 0x01, 0x20, 0x02, 0x30, 0x03, 0x40, 0x04 };

static constexpr uint8_t ServerAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
static constexpr uint8_t ClientAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
static constexpr uint8_t SecretKey[LoLaLinkDefinition::SECRET_KEY_SIZE] = { 0x50, 0x05, 0x60, 0x06, 0x70, 0x07, 0x80, 0x08 };
static constexpr uint8_t SessionId[LoLaLinkDefinition::SESSION_ID_SIZE] = { 0x30, 0x03, 0x33 };

LoLaCryptoAmSession ServerEncoder(SecretKey, AccessPassword, ServerAddress);
LoLaCryptoAmSession ClientEncoder(SecretKey, AccessPassword, ClientAddress);

uint8_t ClientLinkingToken[LoLaLinkDefinition::LINKING_TOKEN_SIZE]{};


static constexpr uint16_t DataSize = LoLaPacketDefinition::GetDataSizeFromPayloadSize(LoLaPacketDefinition::MAX_PAYLOAD_SIZE);

uint8_t RawData[DataSize] = { };
uint8_t Encoded[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE] = { };
uint8_t DecodedData[DataSize] = { };
uint16_t EncodeCounter = 0;



void Halt()
{
	Serial.println("[ERROR] Critical Error");
	delay(1000);

	while (1);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);


	Serial.println(F("LoLa Crypto Unit Tests starting."));

	const bool allTestsOk = PerformUnitTests();

	Serial.println();
	Serial.println();
	if (allTestsOk)
	{
		Serial.println(F("LoLa Crypto Unit Tests completed with success."));
	}
	else
	{
		Serial.println(F("LoLa Crypto Unit Tests FAILED."));
	}
	Serial.println();
}

void PrintHex(const uint8_t value)
{
	if (value < 0x10)
	{
		Serial.print(0, HEX);
	}
	Serial.print(value, HEX);
}


const bool TestUnlinked()
{
	Serial.println(F("RawData"));
	Serial.print('\t');
	Serial.print('|');

	for (uint8_t i = 0; i < DataSize; i++)
	{
		RawData[i] = random((uint32_t)UINT8_MAX + 1);
		PrintHex(RawData[i]);
		Serial.print('|');
	}
	Serial.println();

	ServerEncoder.EncodeOutPacket(RawData, Encoded, EncodeCounter, DataSize);

	Serial.println(F("Encoded"));
	Serial.print('\t');
	Serial.print('|');
	Serial.print('|');
	for (uint8_t i = 0; i < LoLaPacketDefinition::ID_INDEX; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.print('|');
	for (uint8_t i = LoLaPacketDefinition::ID_INDEX; i < LoLaPacketDefinition::DATA_INDEX; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.print('|');
	for (uint8_t i = LoLaPacketDefinition::DATA_INDEX; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.println();

	uint16_t decodeCounter = 0;
	const bool decoded = ClientEncoder.DecodeInPacket(Encoded, DecodedData, decodeCounter, DataSize);
	const bool rejected = !ClientEncoder.DecodeInPacket(Encoded, DecodedData, decodeCounter, DataSize - 1);

	Serial.println(F("DecodedData"));
	Serial.print('\t');
	Serial.print('|');
	for (uint8_t i = 0; i < DataSize; i++)
	{
		PrintHex(DecodedData[i]);
		Serial.print('|');
	}
	Serial.println();

	if (!decoded
		|| !rejected
		|| decodeCounter != EncodeCounter)
	{
		return false;
	}

	for (uint8_t i = 0; i < DataSize; i++)
	{
		if (RawData[i] != DecodedData[i])
		{
			return false;
		}
	}

	EncodeCounter++;

	return true;
}

const bool TestLinking(const uint32_t timestamp = 0)
{
	Serial.println(F("RawData"));
	Serial.print('\t');
	Serial.print('|');

	for (uint8_t i = 0; i < DataSize; i++)
	{
		RawData[i] = random((uint32_t)UINT8_MAX + 1);
		PrintHex(RawData[i]);
		Serial.print('|');
	}
	Serial.println();

	ServerEncoder.EncodeOutPacket(RawData, Encoded, timestamp, EncodeCounter, DataSize);

	Serial.println(F("Encoded"));
	Serial.print('\t');
	Serial.print('|');
	Serial.print('|');
	for (uint8_t i = 0; i < LoLaPacketDefinition::ID_INDEX; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.print('|');
	for (uint8_t i = LoLaPacketDefinition::ID_INDEX; i < LoLaPacketDefinition::DATA_INDEX; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.print('|');
	for (uint8_t i = LoLaPacketDefinition::DATA_INDEX; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
	{
		PrintHex(Encoded[i]);
		Serial.print('|');
	}
	Serial.println();

	uint16_t decodeCounter = 0;
	const bool decoded = ClientEncoder.DecodeInPacket(Encoded, DecodedData, timestamp, decodeCounter, DataSize);
	const bool rejected = !ClientEncoder.DecodeInPacket(Encoded, DecodedData, timestamp, decodeCounter, DataSize - 1);

	Serial.println(F("DecodedData"));
	Serial.print('\t');
	Serial.print('|');
	for (uint8_t i = 0; i < DataSize; i++)
	{
		PrintHex(DecodedData[i]);
		Serial.print('|');
	}
	Serial.println();

	if (!decoded
		|| !rejected
		|| decodeCounter != EncodeCounter)
	{
		return false;
	}

	for (uint8_t i = 0; i < DataSize; i++)
	{
		if (RawData[i] != DecodedData[i])
		{
			return false;
		}
	}

	EncodeCounter++;

	return true;
}

const bool TestLinked()
{
	if (!TestLinking(1))
	{
		return false;
	}

	if (!TestLinking(20))
	{
		return false;
	}

	if (!TestLinking(500))
	{
		return false;
	}

	if (!TestLinking(10000))
	{
		return false;
	}

	if (!TestLinking(INT32_MAX))
	{
		return false;
	}

	if (!TestLinking(UINT32_MAX))
	{
		return false;
	}

	if (!TestLinking(UINT32_MAX - 1))
	{
		return false;
	}

	return true;
}


const bool PerformUnitTests()
{
	if (!ServerEncoder.Setup()
		|| !ClientEncoder.Setup())
	{
		Serial.println(F("Encoder Setup fail."));
		return false;
	}

	ServerEncoder.GenerateProtocolId(
		10000,
		100000,
		0xffffffff);
	ClientEncoder.GenerateProtocolId(
		10000,
		100000,
		0xffffffff);

	ServerEncoder.SetSessionId(SessionId);
	ServerEncoder.SetPartnerAddressFrom(ClientAddress);
	ServerEncoder.ResetAm();

	ClientEncoder.SetSessionId(SessionId);
	ClientEncoder.SetPartnerAddressFrom(ServerAddress);
	ClientEncoder.ResetAm();

	Serial.print(F("Calculating keys."));
	while (!ServerEncoder.Calculate(micros()) || !ClientEncoder.Calculate(micros()))
	{
		Serial.print('.');
	}
	Serial.println();

	if (!ServerEncoder.SessionIsCached()
		|| !ClientEncoder.SessionIsCached())
	{
		Serial.println(F("Encoder.SessionIsCached fail."));
	}

	ClientEncoder.CopyLinkingTokenTo(ClientLinkingToken);
	if (!ServerEncoder.LinkingTokenMatches(ClientLinkingToken))
	{
		Serial.println(F("LinkingTokenMatch fail."));
	}

	bool allTestsOk = true;

	EncodeCounter = UINT16_MAX;
	if (TestUnlinked()
		&& TestUnlinked())
	{
		Serial.println(F("TestUnlinked Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestUnlinked Fail."));
	}
	Serial.println();

	if (TestLinking())
	{
		Serial.println(F("TestLinking Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestLinking Fail."));
	}
	Serial.println();

	if (TestLinked())
	{
		Serial.println(F("TestLinked Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestLinked Fail."));
	}
	Serial.println();

	return allTestsOk;
}

void loop()
{

}

