#include "pch.h"
#include "roIOStream.h"
#include "roRingBuffer.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "roCpuProfiler.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

#if roOS_WIN

struct RawFileIStream : public IStream
{
	RawFileIStream();

			Status		open			(const roUtf8* path);
	virtual bool		readWillBlock	(roUint64 bytesToRead) override;
	virtual Status		size			(roUint64& bytes) const override;
	virtual roUint64	posRead			() const override;
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin) override;
	virtual Status		closeRead		() override;

	void reset();

	void*		_handle;
	bool		_readInProgress;
	OVERLAPPED	_overlap;
	roUint64	_fileSize;
	RingBuffer	_ringBuf;
};	// RawFileIStream

static Status _next_RawFileIStream(IStream& s, roUint64 bytesToRead);

RawFileIStream::RawFileIStream()
{
	_next = _next_RawFileIStream;
	reset();
}

Status RawFileIStream::open(const roUtf8* path)
{
	if(!path)
		return Status::invalid_parameter;

	closeRead();

	Array<roUint16> wstr;
	{	roSize len = 0;
		st = roUtf8ToUtf16(NULL, len, path, roSize(-1)); if(!st) return st;
		if(!wstr.resize(len+1)) return Status::invalid_parameter;
		st = roUtf8ToUtf16(wstr.typedPtr(), len, path, roSize(-1)); if(!st) return st;
		wstr[len] = 0;
	}

	HANDLE h = CreateFileW((wchar_t*)wstr.typedPtr(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if(h == INVALID_HANDLE_VALUE) {
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
			return st = Status::file_not_found;
		return st = Status::file_error;
	}

	_handle = h;
	return st = Status::ok;
}

bool RawFileIStream::readWillBlock(roUint64 size)
{
	if(!_handle)
		return st = Status::file_not_open, false;

	if(_ringBuf.totalReadable() >= size)
		return false;

	// Get file size
	if(_fileSize == 0) {
		if(!(st = this->size(_fileSize)))
			return st;
	}

	size = roMinOf2(_fileSize, size);

CheckProgress:
	DWORD transferred = 0;
	if(_readInProgress) {
		if(!GetOverlappedResult(_handle, &_overlap, &transferred, false)) {
			_ringBuf.commitWrite(0);
			if(GetLastError() == ERROR_IO_PENDING)
				return st = Status::in_progress, true;

			// If EOF or error occurred
			return st = Status::file_ended, false;
		}

		LARGE_INTEGER largeInt;
		largeInt.LowPart = _overlap.Offset;
		largeInt.HighPart = _overlap.OffsetHigh;
		largeInt.QuadPart += transferred;

		_overlap.Offset = largeInt.LowPart;
		_overlap.OffsetHigh = largeInt.HighPart;
		_readInProgress = false;
		_ringBuf.commitWrite(transferred);
		return st = Status::ok, false;
	}

	DWORD bytesToRead = 0;
	st = roSafeAssign(bytesToRead, size);
	if(!st) return false;

	bytesToRead = roMaxOf2(bytesToRead, (DWORD)1024);

	roByte* buf = NULL;
	st = _ringBuf.write(bytesToRead, buf);

	if(::ReadFile(_handle, buf, bytesToRead, NULL, &_overlap)) {
		_readInProgress = true;
		goto CheckProgress;
	}

	if(GetLastError() == ERROR_IO_PENDING) {
		_readInProgress = true;
		goto CheckProgress;
	}

	_ringBuf.commitWrite(0);

	// Other error occurred
	return st = Status::ok, false;
}

static Status _next_RawFileIStream(IStream& s, roUint64 bytesToRead)
{
	RawFileIStream& self = static_cast<RawFileIStream&>(s);

	if(!self._handle)
		Status::file_not_open;

	if(self.current > self.begin)
		self._ringBuf.commitRead(self.current - self.begin);

	while(self.st && self._ringBuf.totalReadable() < bytesToRead) {
		roScopeProfile("rawFileIStream waiting");
		if(self.readWillBlock(bytesToRead)) {
			TaskPool::yield();	// TODO: Do something useful here instead of yield
			continue;
		}
	}

	roSize readableSize = 0;
	self.begin = self._ringBuf.read(readableSize);

	if(readableSize < bytesToRead && readableSize < self._ringBuf.totalReadable()) {
		roSize bytesToFlush = 0;
		if(roSafeAssign(bytesToFlush, bytesToRead - readableSize))
			self._ringBuf.flushWrite(bytesToFlush);
		else
			self._ringBuf.flushWrite();
	}

	self.begin = self._ringBuf.read(readableSize);
	self.current = self.begin;
	self.end = self.begin + readableSize;

	return self.st;
}

Status RawFileIStream::size(roUint64& bytes) const
{
	if(!_handle)
		return Status::file_not_open;

	LARGE_INTEGER fileSize;
	if(!GetFileSizeEx(_handle, &fileSize))
		return Status::file_error;

	bytes = fileSize.QuadPart;
	return Status::ok;
}

roUint64 RawFileIStream::posRead() const
{
	if(!_handle) {
		st = Status::file_not_open;
		return 0;
	}

	LARGE_INTEGER largeInt;
	largeInt.LowPart = _overlap.Offset;
	largeInt.HighPart = _overlap.OffsetHigh;

	return largeInt.QuadPart + (current - begin);
}

Status RawFileIStream::seekRead(roInt64 offset, SeekOrigin origin)
{
	if(!_handle)
		return Status::file_not_open;

	LARGE_INTEGER absOffset = { 0 };
	if(origin == SeekOrigin_Begin) {
		st = roIsValidCast(absOffset.QuadPart, offset);
		if(!st) return st;
		absOffset.QuadPart = offset;
	}
	else if(origin == SeekOrigin_Current) {
		// TODO: Optimization -
		// Prefer seek within the local buffer range: begin, current and end
		absOffset.LowPart = _overlap.Offset;
		absOffset.HighPart = _overlap.OffsetHigh;
		absOffset.QuadPart += offset;
	}
	else if(origin == SeekOrigin_End) {
		if(!GetFileSizeEx(_handle, &absOffset))
			return Status::file_seek_error;
		absOffset.QuadPart -= offset;
	}
	else {
		roAssert(false);
		return Status::invalid_parameter;
	}

	_readInProgress = false;
	_overlap.Internal = _overlap.InternalHigh = 0;
	_overlap.Offset = absOffset.LowPart;
	_overlap.OffsetHigh = absOffset.HighPart;

	_ringBuf.clear();

	return Status::ok;
}

Status RawFileIStream::closeRead()
{
	if(_handle)
		roVerify(CloseHandle(_handle));

	reset();
	return Status::ok;
}

void RawFileIStream::reset()
{
	_handle = NULL;
	_readInProgress = false;
	roMemZeroStruct(_overlap);
	_fileSize = 0;
	_ringBuf.clear();
}

#else

struct RawFileIStream : public IStream
{
	RawFileIStream();

			Status		open			(const roUtf8* path);
	virtual bool		readWillBlock	(roUint64 bytesToRead);
	virtual Status		size			(roUint64& bytes) const;
	virtual roUint64	posRead			() const;
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin);
	virtual void		closeRead		();

	FILE*		_file;
	RingBuffer	_ringBuf;
};	// RawFileIStream

static Status _next_RawFileIStream(IStream& s, roUint64 bytesToRead);

RawFileIStream::RawFileIStream()
{
	_file = NULL;
	_next = _next_RawFileIStream;
}

Status RawFileIStream::open(const roUtf8* path)
{
	if(!path) return Status::invalid_parameter;

	closeRead();

	_file = fopen(path, "rb");
	if(!_file)
		return st = Status::file_not_found;

	return st = Status::ok;
}

bool RawFileIStream::readWillBlock(roUint64 size)
{
	return false;
}

static Status _next_RawFileIStream(IStream& s, roUint64 bytesToRead)
{
	RawFileIStream& self = static_cast<RawFileIStream&>(s);

	if(self.current > self.begin)
		self._ringBuf.commitRead(self.current - self.begin);

	if(!self._file)
		return Status::file_not_open;

	while(self.st && self._ringBuf.totalReadable() < bytesToRead) {
		roByte* wPtr = NULL;
		self.st = self._ringBuf.write(bytesToRead, wPtr);
		if(self.st) {
			roUint64 bytesRead = num_cast<roUint64>(fread(wPtr, 1, bytesToRead, self._file));
			self._ringBuf.commitWrite(bytesRead);

			if(bytesRead == 0)
				self.st = Status::file_ended;
		}
		else
			self._ringBuf.commitWrite(0);
	}

	roSize readableSize = 0;
	self.begin = self._ringBuf.read(readableSize);

	if(readableSize < bytesToRead && readableSize < self._ringBuf.totalReadable()) {
		roSize bytesToFlush = 0;
		if(roSafeAssign(bytesToFlush, bytesToRead - readableSize))
			self._ringBuf.flushWrite(bytesToFlush);
		else
			self._ringBuf.flushWrite();
	}

	self.begin = self._ringBuf.read(readableSize);
	self.current = self.begin;
	self.end = self.begin + readableSize;

	return self.st;
}

Status RawFileIStream::size(roUint64& bytes) const
{
	if(!_file)
		Status::file_not_open;

	struct stat st;
	if(fstat(fileno(_file), &st) != 0)
		return Status::file_error;

	bytes = num_cast<roUint64>(st.st_size);

	return Status::ok;
}

roUint64 RawFileIStream::posRead() const
{
	if(!_file) {
		st = Status::file_not_open;
		return 0;
	}

	return ftell(_file);
}

Status RawFileIStream::seekRead(roInt64 offset, SeekOrigin origin)
{
	if(!_file)
		Status::file_not_open;

	Status st = roIsValidCast<long>(offset);
	if(!st) return st;

	if(fseek(_file, num_cast<long>(offset), origin) == 0)
		st = Status::ok;
	else
		st = Status::file_seek_error;

	_ringBuf.clear();

	return st;
}

void RawFileIStream::closeRead()
{
	if(_file)
		fclose(_file);

	_file = NULL;
	_ringBuf.clear();
}

#endif

Status openRawFileIStream(roUtf8* path, AutoPtr<IStream>& stream, bool blocking)
{
	AutoPtr<RawFileIStream> s = defaultAllocator.newObj<RawFileIStream>();
	if(!s.ptr()) return Status::not_enough_memory;

	Status st = s->open(path);

	if(blocking && st == Status::in_progress) {
		while(s->readWillBlock(1)) {}
		st = s->st;
	}

	if(!st)
		return st;

	stream = std::move(s);
	return st;
}

}	// namespace ro
