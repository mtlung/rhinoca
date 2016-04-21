#include "pch.h"
#include "roHttpFileSystem.h"
#include "roLog.h"
#include "roArray.h"
#include "roStopWatch.h"
#include "roString.h"
#include "roStringFormat.h"
#include "roStringUtility.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../math/roMath.h"
#include "../network/roSocket.h"
#include "../platform/roPlatformHeaders.h"
#include <stdio.h>

namespace ro {

// Perform character encoding
// http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
static const char* _encode[] = { "%20", "%22", "%23", "%24", "%25", "%26", "%3C", "%3E" };
static const char _decode[] = " \"#$%&<>";

static String _encodeUrl(const char* uri)
{
	roSize orgLen = roStrLen(uri);
	if(orgLen == 0) return NULL;

	String ret;
	ret.resize(orgLen * 3 + 1);
	char* buf = ret.c_str();

	do {
		bool converted = false;
		for(roSize i=0; i <= sizeof(_decode); ++i) {
			if(*uri == _decode[i]) {
				for(const char* s = _encode[i]; *s != '\0'; ++s)
					*(buf++) = *s;
				converted = true;
				break;
			}
		}
		if(!converted)
			*(buf++) = *uri;
	} while(*(++uri) != '\0');

	*buf = '\0';
	ret.condense();
	return ret;
}

enum HttpState {
	State_Error,
	State_SendRequest,
	State_ReadingResponse,
	State_RecivedResponse,
	State_ReadingChunkSize,
	State_LastDataReceived,
	State_ReadReady,
	State_DataEnd,
};

/// Notes on http 1.0 protocol:
/// Http 1.0 protocol contains an optional "Content-Length" attribute, but
/// if it's not present, the end of the data will indicated by a graceful disconnection.
/// See: http://www.xml.com/pub/a/ws/2003/11/25/protocols.html
/// HTTP Make Really Easy
/// http://www.jmarshall.com/easy/http/
/// A list of HTTP client library
/// http://curl.haxx.se/libcurl/competitors.html
struct HttpStream
{
	~HttpStream() { defaultAllocator.free(buffer); }
	BsdSocket socket;
	char* buffer;
	HttpState state;
	Status status;
	Status getSizeStatus;
	bool chunked;
	roSize chunkSizeToRead;
	roSize chunkHeaderOffset;	///< Offset into buffer where the chunk header begin
	roSize bufSize;
	roSize bufCapacity;
	roSize nextTrunkSize;
	roSize expectedTotalSize;	///< Rely on Content-Length, 
	roSize userReadCount;		///< Amount of bytes consumed by httpFileSystemRead()
	roSize lastReadSize;		///< Size of readable returned by httpFileSystemGetBuffer()

	String getCmd;

	StopWatch stopWatch;
};

static Status prepareForRead(HttpStream& s, roUint64 readSize)
{
	int reserveForNull = 1;

	// Move data remains in s->buffer to the front
	if(s.lastReadSize > 0) {
		roMemmov(s.buffer, s.buffer + s.lastReadSize, s.bufSize - s.lastReadSize);
		s.bufSize -= s.lastReadSize;
		s.lastReadSize = 0;
	}

	// Detect integer overflow
	if(TypeOf<roSize>::valueMax() - s.bufSize - reserveForNull < readSize)
		return s.status = Status::arithmetic_overflow;

	roSize size = num_cast<roSize>(readSize);

	// Enlarge the buffer if necessary
	if(s.bufCapacity < s.bufSize + size + reserveForNull) {
		// TODO: realloc perform copy for the whole buffer while it's not necessary here.
		roBytePtr p = defaultAllocator.realloc(s.buffer, s.bufCapacity, s.bufSize + size + reserveForNull);
		if(!p) return s.status = Status::not_enough_memory;
		s.buffer = p;
		s.bufCapacity = s.bufSize + size + reserveForNull;
	}

	return s.status = Status::ok;
}

static bool _parseUri(const char* uri, SockAddr& addr, Array<char>& host, String& requestStr)
{
	String _uri = _encodeUrl(uri);

	// Parse http://host
	Array<char> path;
	host.resize(_uri.size(), 0);
	path.resize(_uri.size(), 0);
	if(sscanf(_uri.c_str(), "http://%[^/]%s", host.begin(), path.begin()) < 1) {
		roLog("error", "Invalid uri: %s\n", _uri.c_str());
		return false;
	}

	host.condense();
	path.condense();

	if(path[0] == '\0')
		path.insert(0, '/');

	// NOTE: Currently this host resolving operation is blocking
	bool parseOk = false;
	if(host.find(':'))
		parseOk = addr.parse(host.begin());
	else
		parseOk = addr.parse(host.begin(), 80);

	if(!parseOk) {
		roLog("error", "Fail to resolve host %s\n", host.begin());
		return false;
	}

	const char getFmt[] =
		"GET {} HTTP/1.1\r\n"
		"Host: {}\r\n"	// Required for http 1.1
		"User-Agent: The Roar Engine\r\n"
//		"Range: bytes=0-64\r\n"
		"\r\n";

	requestStr.clear();
	strFormat(requestStr, getFmt, path.begin(), host.begin());

	return true;
}

static Status _makeConnection(HttpStream& s, const char* uri)
{
	s.buffer = NULL;
	s.state = State_SendRequest;
	s.chunked = false;
	s.chunkSizeToRead = TypeOf<roSize>::valueMax();
	s.chunkHeaderOffset = 0;
	s.bufSize = 0;
	s.bufCapacity = 0;
	s.nextTrunkSize = 0;
	s.expectedTotalSize = 0;
	s.userReadCount = 0;
	s.lastReadSize = 0;

	SockAddr addr;
	Array<char> host;
	if(!_parseUri(uri, addr, host, s.getCmd)) {
		s.state = State_Error;
		return s.status = Status::http_invalid_uri;
	}

	// Create socket
	s.status = s.socket.create(BsdSocket::TCP);
	if(!s.status) return s.status;

	s.socket.setBlocking(false);

	// Make connection
	s.status = s.socket.connect(addr);
	if(BsdSocket::isError(s.status)) {
		roLog("error", "Connection to %s failed\n", host.begin());
		s.state = State_Error;
		return s.status = Status::http_error;
	}

	return s.status = Status::ok;
}

static void _closeConnection(HttpStream& s)
{
	s.socket.close();
}

static Status _readFromSocket(HttpStream& s, roSize size, roSize* bytesRead=NULL)
{
	roSize inSize = size;
	Status st = prepareForRead(s, size); if(!st) return st;
	st = s.socket.receive(s.buffer + s.bufSize, size);

	if(bytesRead)
		*bytesRead = size;

	if(!st) return st;
	if(inSize > 0 && size == 0) return Status::file_ended;

	s.bufSize += size;
	return Status::ok;
}

Status httpFileSystemOpenFile(const char* uri, void*& outFile)
{
	AutoPtr<HttpStream> s = defaultAllocator.newObj<HttpStream>();

	Status st = _makeConnection(*s, uri); if(!st) return st;

	s->buffer = NULL;
	s->getSizeStatus = Status::http_unknow_size;

	outFile = s.unref();
	return Status::ok;
}

bool httpFileSystemReadWillBlock(void* file, roUint64 size)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s) return false;

	roSize requestSize = 0;
	s->status = roSafeAssign(requestSize, size);
	if(!s->status) return false;

	static const roSize headerBufSize = 128;

// Http state handling

ConnectionRestart:
	if(s->state == State_Error)
		return false;

// Send http request
	if(s->state == State_SendRequest)
	{
		roSize cmdSize = s->getCmd.size();
		s->status = s->socket.send(s->getCmd.c_str(), cmdSize);
		if(s->status == roStatus::net_notconn)
			return true;

		if(BsdSocket::isError(s->status))
			return false;

		s->state = State_ReadingResponse;
	}

// Reading server respond header
	if(s->state == State_ReadingResponse)
	{
		s->status = _readFromSocket(*s, headerBufSize);
		if(s->status == Status::in_progress) return true;
		if(!s->status) return false;

		// Make sure the buffer are always null terminated
		s->buffer[s->bufSize] = '\0';

		// Check if header complete
		static const char headerTerminator[] = "\r\n\r\n";

		char* messageContent = roStrStr(s->buffer, headerTerminator);
		if(!messageContent)	// We need to read more till we see the header terminator
			return true;

		messageContent += (sizeof(headerTerminator) - 1);	// -1 for the null terminator

		// Nullify the separator between header and body
		messageContent[-1] = '\0';

		// Check for status
		int serverRetCode = 0;
		if(sscanf(s->buffer, "HTTP/%*c.%*c %d %*[^\n]", &serverRetCode) != 1)
			return s->status = Status::http_error, false;

		// Check for server return code
		// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
		if(serverRetCode == 200)	// Ok
		{
			// See what's the encoding
			if(const char* p = roStrStr(s->buffer, "Transfer-Encoding:")) {
				char encoding[64];	// TODO: Not safe
				if(sscanf(p, "Transfer-Encoding:%*[ \t]%s", encoding) != 1)
					return s->status = Status::http_error, false;

				s->chunked = roStrStr(encoding, "chunked") != NULL;
			}

			// Get the content length
			if(const char* p = roStrStr(s->buffer, "Content-Length:")) {
				if(sscanf(p, "Content-Length:%*[ \t]%zu", &s->expectedTotalSize) != 1)
					return s->status = Status::http_error, false;

				s->getSizeStatus = Status::ok;
			}
			else
				s->expectedTotalSize = roSize(-1);

			// Move the received (if any) body content to the beginning of our buffer
			roSize bodySize = s->bufSize - (messageContent - s->buffer);
			roMemmov(s->buffer, messageContent, bodySize);
			s->bufSize = bodySize;

			if(s->chunked) {
				s->state = State_ReadingChunkSize;
				s->chunkHeaderOffset = 0;
			}
			else
				s->state = State_ReadReady;
		}
		else if(serverRetCode == 206)		// Partial Content
		{
			serverRetCode = 206;
		}
		else if(
			serverRetCode == 301 ||			// Moved Permanently
			serverRetCode == 302			// Found (http redirect)
		)
		{
			if(const char* p = roStrStr(s->buffer, "Location:")) {
				char address[256];	// TODO: Not safe
				if(sscanf(p, "Location:%*[ \t]%s", address) != 1)
					return s->status = Status::http_error, false;

				_closeConnection(*s);
				s->status = _makeConnection(*s, address);
				if(!s->status)
					return false;

				goto ConnectionRestart;
			}

			return s->status = Status::http_error, false;
		}
		else
		{
			s->status = Status::http_error;
			roLog("error", "Http stream receive server error code '%d'\n", serverRetCode);
			return s->status = Status::http_error, false;
		}
	}

// Reading chunk size
	if(s->state == State_ReadingChunkSize)
	{
		static const char chunkSizeTerminator[] = "\r\n";
		static const roSize ignoreBeginningCRLF = 1;
		char* pTerminator = NULL;

		// Read byte by byte till we find "\r\n"
		while(true) {
			pTerminator = roStrnStr(
				s->buffer + s->chunkHeaderOffset + ignoreBeginningCRLF,
				roClampedSubtraction(s->bufSize, s->chunkHeaderOffset + ignoreBeginningCRLF),
				chunkSizeTerminator
			);

			if(pTerminator)
				break;

			s->status = _readFromSocket(*s, 1);
			if(s->status == Status::in_progress) return true;
			if(!s->status) return false;
		}

		int count;
		char n, r;
		if(sscanf(s->buffer + s->chunkHeaderOffset, "%x%c%c", &count, &n, &r) != 3)
			return s->status = Status::http_error, false;

		// Trim out the header in the buffer
		char* messageContent = pTerminator + roStrLen(chunkSizeTerminator);
		roSize sizeToMove = s->buffer + s->bufSize - messageContent;
		roSize headerSize = messageContent - (s->buffer + s->chunkHeaderOffset);
		roMemmov(s->buffer + s->chunkHeaderOffset, messageContent, sizeToMove);

		s->bufSize -= headerSize;
		s->chunkSizeToRead = count - sizeToMove;

		s->state = State_ReadReady;

		// Check if it's the last chunk
		if(count == 0) {
			s->state = State_LastDataReceived;
			s->expectedTotalSize = s->userReadCount + s->bufSize;
		}
	}

	if(s->state == State_LastDataReceived)
	{
		if(s->bufSize == 0) {
			s->state = State_DataEnd;
			return s->status = Status::file_ended, false;
		}
	}

// Really read the content to the buffer
	roAssert(s->state > State_ReadingResponse);

	if(s->userReadCount + s->bufSize >= s->expectedTotalSize)
		s->state = State_LastDataReceived;

	if(s->state == State_LastDataReceived)
		return false;

	// Else we try to read more from the socket, but we never read from socket more than
	// the chunk size, and the requested size, this can simplify the code a lot
	roSize bytesToRead = roMinOf2(
		roClampedSubtraction(requestSize, s->bufSize),
		s->chunkSizeToRead
	);

	roSize bytesRead = 0;
	s->status = _readFromSocket(*s, bytesToRead, &bytesRead);
	if(s->status == Status::in_progress) return true;
	if(!s->status) return false;

	if(s->chunked) {
		s->chunkSizeToRead -= bytesRead;
		s->chunkHeaderOffset = s->bufSize;

		if(s->chunkSizeToRead == 0)
			s->state = State_ReadingChunkSize;
	}

	return s->bufSize < requestSize;
}

static Status _blockTillReadable(HttpStream& s, roUint64 size)
{
	if(size == 0) return Status::ok;
	Status st = prepareForRead(s, size); if(!st) return st;

	float timeout = 10;
	while(s.state != State_ReadReady || s.bufSize < size) {
		if(!httpFileSystemReadWillBlock(&s, size))
			break;

		TaskPool::yield();	// TODO: Do something useful here instead of yield
		if(s.state == State_ReadReady)
			continue;

		// Detect timeout for connecting to remote host
		timeout -= (float)s.stopWatch.getAndReset();
/*		if(timeout <= 0) {
			s.state = State_Error;
			roLog("warn", "Connection to %s time out\n", "http");	// TODO: Print out the URL
			return false;
		}*/
	}

	return s.status;
}

Status httpFileSystemRead(void* file, void* buffer, roUint64 size, roUint64& bytesRead)
{
	bytesRead = 0;

	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s) return Status::invalid_parameter;

	roSize _size = 0;
	s->status = roSafeAssign(_size, size);
	if(!s->status) return s->status;

	_blockTillReadable(*s, _size);
	roAssert(s->status != Status::in_progress);
	if(!s->status) return s->status;

	roAssert(s->state == State_ReadReady || s->state == State_LastDataReceived);
	roAssert(s->userReadCount < s->expectedTotalSize);

	roSize bytesToRead = roMinOf2(s->bufSize, _size);

	// Copy s->buffer to destination
	roMemcpy(buffer, s->buffer, bytesToRead);

	s->userReadCount += bytesToRead;
	s->lastReadSize = bytesToRead;

//	printf("read: %u\n", s->userReadCount);

	bytesRead = bytesToRead;
	return s->status;
}

Status httpFileSystemAtomicRead(void* file, void* buffer, roUint64 size)
{
	roUint64 bytesRead = 0;
	Status st = httpFileSystemRead(file, buffer, size, bytesRead);
	if(!st) return st;
	if(bytesRead < size) return Status::file_ended;
	return st;
}

Status httpFileSystemSize(void* file, roUint64& size)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s) return Status::invalid_parameter;

	if(httpFileSystemReadWillBlock(file, 1)) return Status::in_progress;
	if(!s->getSizeStatus) return s->getSizeStatus;

	size = s->expectedTotalSize;
	return Status::ok;
}

Status httpFileSystemSeek(void* file, roInt64 offset, FileSystem::SeekOrigin origin)
{
	return Status::not_implemented;
}

void httpFileSystemCloseFile(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	defaultAllocator.deleteObj(s);
}

roBytePtr httpFileSystemGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s || s->state == State_Error || requestSize == 0) { readableSize = 0; return NULL; }

	roSize bytesToRead = 0;
	if(!roSafeAssign(bytesToRead, requestSize)) return false;

	if(!_blockTillReadable(*s, bytesToRead))
		bytesToRead = 0;

	readableSize = bytesToRead = roMinOf2(s->bufSize, bytesToRead);
	s->userReadCount += bytesToRead;
	s->lastReadSize = bytesToRead;

	return bytesToRead > 0 ? s->buffer : NULL;
}

void httpFileSystemTakeBuffer(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s || s->state == State_Error || s->lastReadSize == 0) return;

	roBytePtr newBuf = defaultAllocator.malloc(s->bufCapacity);
	s->buffer = newBuf;

	if(!newBuf) {
		s->state = State_Error;
		return;
	}

	roMemcpy(newBuf, s->buffer + s->lastReadSize, s->bufSize - s->lastReadSize);
	s->bufSize -= s->lastReadSize;
	s->lastReadSize = 0;
}

void httpFileSystemUntakeBuffer(void* file, roBytePtr buf)
{
	defaultAllocator.free(buf);
}

void* httpFileSystemOpenDir(const char* uri)
{
	roAssert(false);
	return NULL;
}

bool httpFileSystemNextDir(void* dir)
{
	roAssert(false);
	return false;
}

const char* httpFileSystemDirName(void* dir)
{
	roAssert(false);
	return "";
}

void httpFileSystemCloseDir(void* dir)
{
}


// ----------------------------------------------------------------------

FileSystem httpFileSystem = {
	httpFileSystemOpenFile,
	httpFileSystemReadWillBlock,
	httpFileSystemRead,
	httpFileSystemAtomicRead,
	httpFileSystemSize,
	httpFileSystemSeek,
	httpFileSystemCloseFile,
	httpFileSystemGetBuffer,
	httpFileSystemTakeBuffer,
	httpFileSystemUntakeBuffer,
	httpFileSystemOpenDir,
	httpFileSystemNextDir,
	httpFileSystemDirName,
	httpFileSystemCloseDir
};

}	// namespace ro
