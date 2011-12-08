#include "pch.h"
#include "roHttpFileSystem.h"
#include "roLog.h"
#include "roArray.h"
#include "roSocket.h"
#include "roStopWatch.h"
#include "roString.h"
#include "roStringUtility.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include <stdio.h>

namespace ro {

static DefaultAllocator _allocator;

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
	~HttpStream() { _allocator.free(buffer); }
	BsdSocket socket;
	char* buffer;
	bool headerSent;
	bool headerReceived;
	bool httpError;
	roSize bufSize;
	roSize bufCapacity;
	roSize expectedTotalSize;	///< Rely on Content-Length, 
	roSize userReadCount;		///< Amount of bytes consumed by httpFileSystemRead()
	roSize lastReadSize;		///< Size of readable returned by httpFileSystemGetBuffer()

	String getCmd;

	StopWatch stopWatch;
};

static bool prepareForRead(HttpStream* s, roSize readSize)
{
	int reserveForNull = 1;

	// Move data remains in s->buffer to the front
	if(s->lastReadSize > 0) {
		roMemmov(s->buffer, s->buffer + s->lastReadSize, s->bufSize - s->lastReadSize);
		s->bufSize -= s->lastReadSize;
		s->lastReadSize = 0;
	}

	// Detect integer overflow
	if(TypeOf<roSize>::valueMax() - s->bufSize - reserveForNull < readSize)
		return false;

	// Enlarge the buffer if necessary
	if(s->bufCapacity < s->bufSize + readSize + reserveForNull) {
		// TODO: realloc perform copy for the whole buffer while it's not necessary here.
		roBytePtr p = _allocator.realloc(s->buffer, s->bufCapacity, s->bufSize + readSize + reserveForNull);
		if(!p) return false;
		s->buffer = p;
		s->bufCapacity = s->bufSize + readSize + reserveForNull;
	}

	return true;
}

void* httpFileSystemOpenFile(const char* uri)
{
	String _uri = _encodeUrl(uri);

	// Parse http://host
	Array<char> host;
	Array<char> path;
	host.resize(_uri.size(), 0);
	path.resize(_uri.size(), 0);
	if(sscanf(_uri.c_str(), "http://%[^/]%s", host.begin(), path.begin()) < 1)
		return NULL;

	host.condense();
	path.condense();

	if(path[0] == '\0')
		path.insert(0, '/');

	const char getFmt[] =
		"GET %s HTTP/1.0\r\n"
		"Host: %s\r\n"	// Required for http 1.1
		"User-Agent: The Roar Engine\r\n"
		"\r\n";

    int ret = 0;
	SockAddr adr;
	AutoPtr<HttpStream> s = _allocator.newObj<HttpStream>();

	s->getCmd.sprintf(getFmt, path.begin(), host.begin());

	s->buffer = NULL;
	s->headerSent = false;
	s->headerReceived = false;
	s->httpError = false;
	s->bufSize = 0;
	s->bufCapacity = 0;
	s->expectedTotalSize = 0;

	s->userReadCount = 0;
	s->lastReadSize = 0;

	// NOTE: Currently this host resolving operation is blocking
	if(!adr.parse(host.begin(), 80))
		goto OnError;

	roVerify(s->socket.create(BsdSocket::TCP) == 0);
	s->socket.setBlocking(false);

	ret = s->socket.connect(adr);
	if(ret != 0 && !BsdSocket::inProgress(s->socket.lastError))
		goto OnError;

	return s.unref();

OnError:
	roLog("error", "Connection to %s failed\n", host);
	return NULL;
}

bool httpFileSystemReadReady(void* file, roUint64 size)
{
	roAssert(file);
	roSize _size = clamp_cast<roSize>(size);

    int ret = 0;
	HttpStream* s = reinterpret_cast<HttpStream*>(file);

	static const roSize headerBufSize = 128;

	if(s->httpError)
		goto OnError;

	if(!s->headerSent) {
		ret = s->socket.send(s->getCmd.c_str(), s->getCmd.size());
		if(ret < 0 && s->socket.lastError == ENOTCONN)
			return false;

		if(ret < 0 && !BsdSocket::inProgress(s->socket.lastError))
			goto OnError;

		s->headerSent = true;
	}

	if(!s->headerReceived) {
		if(!prepareForRead(s, headerBufSize))
			goto OnError;

		ret = s->socket.receive(s->buffer + s->bufSize, headerBufSize);
		if(ret < 0 && BsdSocket::inProgress(s->socket.lastError))
			return false;
		if(ret == 0)
			goto OnEof;

		s->bufSize += ret;

		// Make sure the buffer are always null terminated
		s->buffer[s->bufSize] = '\0';

		// Check if header complete
		static const char headerTerminator[] = "\r\n\r\n";

		char* messageContent = roStrStr(s->buffer, headerTerminator);
		if(!messageContent)
			return false;

		messageContent += (sizeof(headerTerminator) - 1);	// -1 for the null terminator

		// Nullify the separator between header and body
		messageContent[-1] = '\0';

		// Check for status
		int serverRetCode = 0;
		if(sscanf(s->buffer, "HTTP/%*c.%*c %d %*[^\n]", &serverRetCode) != 1)
			goto OnError;

		if( serverRetCode == 302 ||	// Found (http redirect)
			serverRetCode == 200)	// Ok
		{
			// Get the content length
			if(const char* p = roStrStr(s->buffer, "Content-Length:")) {
				if(sscanf(p, "Content-Length:%u", &s->expectedTotalSize) != 1)
					goto OnError;
			}
			else
				s->expectedTotalSize = roSize(-1);

			roSize bodySize = s->bufSize - (messageContent - s->buffer);
			// Move the received (if any) body content to the beginning of our buffer
			roMemmov(s->buffer, messageContent, bodySize);
			s->bufSize = bodySize;

			s->headerReceived = true;
		}
		else
		{
			roLog("error", "Http stream receive server error code '%d'\n", serverRetCode);
			goto OnError;
		}
	}

	if(s->bufSize >= _size)
		return true;

	roAssert(s->headerReceived);

	if(!prepareForRead(s, _size - s->bufSize))
		goto OnError;
	ret = s->socket.receive(s->buffer + s->bufSize, _size - s->bufSize);
	if(ret < 0 && BsdSocket::inProgress(s->socket.lastError))
		return false;
	if(ret < 0)
		goto OnError;
	if(ret == 0)
		goto OnEof;

	s->bufSize += ret;

	return s->bufSize >= _size;

OnEof:
	return true;

OnError:
	roLog("error", "read failed\n");
	s->httpError = true;
	return true;
}

static bool _blockTillReadable(HttpStream* s, roSize size)
{
	if(size == 0) return true;
	if(!prepareForRead(s, size)) {
		s->httpError = true;
		return false;
	}

	float timeout = 300000;
	while(!s->headerReceived || s->bufSize < size) {
		if(httpFileSystemReadReady(s, size))
			break;

		TaskPool::sleep(0);	// TODO: Do something usefull here instead of sleep
		if(s->headerReceived)
			continue;

		// Detect timeout for connecting to remote host
		timeout -= (float)s->stopWatch.getAndReset();
		if(timeout <= 0) {
			s->httpError = true;
			return false;
		}
	}

	return true;
}

roUint64 httpFileSystemRead(void* file, void* buffer, roUint64 size)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	roSize _size = clamp_cast<roSize>(size);

	if(!s || _size == 0) return 0;
	if(s->httpError) return 0;
	if(!_blockTillReadable(s, _size)) return 0;

	if(!s->headerReceived) return 0;
	if(s->userReadCount >= s->expectedTotalSize) return 0;

	roSize bytesToRead = roMinOf2(s->bufSize, _size);

	// Copy s->buffer to destination
	roMemcpy(buffer, s->buffer, bytesToRead);

	s->userReadCount += bytesToRead;
	s->lastReadSize = bytesToRead;

//	printf("read: %u\n", s->userReadCount);

	return bytesToRead;
}

roUint64 httpFileSystemSize(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s || s->httpError || !_blockTillReadable(s, 1)) return 0;
	return s->expectedTotalSize;
}

int httpFileSystemSeek(void* file, roUint64 offset, FileSystem::SeekOrigin origin)
{
	return -1;
}

void httpFileSystemCloseFile(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	_allocator.deleteObj(s);
}

roBytePtr httpFileSystemGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s || s->httpError || requestSize == 0) { readableSize = 0; return NULL; }
	roSize bytesToRead = clamp_cast<roSize>(requestSize);

	if(!_blockTillReadable(s, bytesToRead))
		bytesToRead = 0;

	readableSize = bytesToRead = roMinOf2(s->bufSize, bytesToRead);
	s->userReadCount += bytesToRead;
	s->lastReadSize = bytesToRead;

	return bytesToRead > 0 ? s->buffer : NULL;
}

void httpFileSystemTakeBuffer(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	if(!s || s->httpError || s->lastReadSize == 0) return;

	roBytePtr newBuf = _allocator.malloc(s->bufCapacity);
	s->buffer = newBuf;

	if(!newBuf) {
		s->httpError = true;
		return;
	}

	roMemcpy(newBuf, s->buffer + s->lastReadSize, s->bufSize - s->lastReadSize);
	s->bufSize -= s->lastReadSize;
	s->lastReadSize = 0;
}

void httpFileSystemUntakeBuffer(void* file, roBytePtr buf)
{
	_allocator.free(buf);
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
	httpFileSystemReadReady,
	httpFileSystemRead,
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
