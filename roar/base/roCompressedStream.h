#ifndef __roCompressedStream_h__
#define __roCompressedStream_h__

#include "roIOStream.h"
#include "../../thirdParty/zlib/zlib.h"

namespace ro {

struct GZipIStream : public IStream
{
	~GZipIStream();

	Status init(AutoPtr<IStream>&& stream, roSize chunkSize=1024 * 4);

	virtual	Status read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead) override;

// Private
	z_stream _zStream;
	ByteArray _inBuffer;
	AutoPtr<IStream> _innerStream;
};	// GZipIStream

struct GZipOStream : public OStream
{
	~GZipOStream();

	Status init(AutoPtr<OStream>&& stream, roSize chunkSize = 1024 * 4);

	virtual	Status		write		(const void* buffer, roUint64 bytesToWrite) override;
	virtual roUint64	posWrite	() const override;
	virtual Status		flush		() override;
	virtual Status		closeWrite	() override;

// Private
	z_stream _zStream;
	ByteArray _buffer;
	AutoPtr<OStream> _innerStream;
};	// GZipOStream


}   // namespace ro

#endif	// __roCompressedStream_h__
