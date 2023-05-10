// Codec2StreamWriter.h

#ifndef _CODEC2STREAMWRITER_h
#define _CODEC2STREAMWRITER_h

class Codec2StreamWriter
{
private:
	struct StreamChunkCodec2Mode3200
	{
		uint8_t Data[64];
	}

	struct StreamChunkCodec2Mode2200
	{
		uint8_t Data[56];
	}


	TemplateTrackedStreamBuffer <10, StreamChunkCodec2Mode3200> ExperimentalTrackedBuffer;
}

#endif

