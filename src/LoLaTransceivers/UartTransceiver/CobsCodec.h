#ifndef _COBS_CODEC_h
#define _COBS_CODEC_h

#include <stdint.h>

namespace CobsCodec
{
	static constexpr uint8_t MessageEnd = 0;

	static const uint8_t Encode(const uint8_t* buffer, uint8_t* encodedBuffer, const uint8_t size)
	{
		static constexpr uint8_t endReplacement = MessageEnd + 1;

		uint8_t read_index = 0;
		uint8_t write_index = 1;
		uint8_t code_index = 0;
		uint8_t code = 1;

		while (read_index < size)
		{
			if (buffer[read_index] == MessageEnd)
			{
				encodedBuffer[code_index] = code;
				code = endReplacement;
				code_index = write_index++;
				read_index++;
			}
			else
			{
				encodedBuffer[write_index++] = buffer[read_index++];
				code++;

				if (code == 0xFF)
				{
					encodedBuffer[code_index] = code;
					code = endReplacement;
					code_index = write_index++;
				}
			}
		}

		encodedBuffer[code_index] = code;

		return write_index;
	}

	static const uint8_t Decode(const uint8_t* buffer, uint8_t* decodedBuffer, const uint8_t size)
	{
		static constexpr uint8_t endReplacement = MessageEnd + 1;

		if (size == 0)
		{
			return 0;
		}

		uint8_t read_index = 0;
		uint8_t write_index = 0;
		uint8_t code = 0;
		uint8_t i = 0;

		while (read_index < size)
		{
			code = buffer[read_index];

			if (read_index + code > size && code != endReplacement)
			{
				return 0;
			}

			read_index++;

			for (i = endReplacement; i < code; i++)
			{
				decodedBuffer[write_index++] = buffer[read_index++];
			}

			if (code != 0xFF && read_index != size)
			{
				decodedBuffer[write_index++] = MessageEnd;
			}
		}

		return write_index;
	}
};
#endif