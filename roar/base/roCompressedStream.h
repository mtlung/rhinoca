#ifndef __roCompressedStream_h__
#define __roCompressedStream_h__

#include "roIOStream.h"
#include "../../thirdParty/zlib/zlib.h"

namespace ro {

struct GZipStreamIStream : public IStream
{
	~GZipStreamIStream();

	Status init(AutoPtr<IStream>& stream, roSize chunkSize=1024 * 4);

	virtual	Status read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead) override;

// Private
	z_stream _zStream;
	ByteArray _inBuffer;
	AutoPtr<IStream> _innerStream;
};	// GZipStreamIStream

}   // namespace ro

#endif	// __roCompressedStream_h__
