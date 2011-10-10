#include "pch.h"
#include "httpstream.h"
#include "socket.h"
#include "taskpool.h"
#include "timer.h"

#include <string.h>

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
	~HttpStream() { rhinoca_free(buffer); }
	BsdSocket socket;
	char* buffer;
	bool headerSent;
	bool headerReceived;
	bool httpError;
	unsigned bufSize;
	unsigned bufCapacity;
	unsigned expectedTotalSize;	/// Rely on Content-Length, 
	unsigned userReadCount;	/// Amount of bytes consumed by rhinoca_http_read()

	char getCmd[256];

	Rhinoca* rh;
	DeltaTimer timer;
};

static void prepareForRead(HttpStream* s, unsigned readSize)
{
	int reserveForNull = 1;
	if(s->bufCapacity <= s->bufSize + readSize + reserveForNull) {
		s->buffer = (char*)rhinoca_realloc(s->buffer, s->bufCapacity, s->bufSize + readSize + reserveForNull);
		s->bufCapacity = s->bufSize + readSize + reserveForNull;
	}
}

void* rhinoca_http_open(Rhinoca* rh, const char* uri)
{
	// Perform character encoding
	// http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
	const char* encode[] = { "%20", "%22", "%23", "%24", "%25", "%26", "%3C", "%3E" };
	uri = replaceCharacterWithStr(uri, " \"#$%&<>", encode);

	// Parse http://host
	// NOTE: Buffer overflow may occur for sscanf
	char host[128] = "";
	char path[512] = "";
	sscanf(uri, "http://%[^/]%s", host, path);

	unsigned uriLen = strlen(uri);
	rhinoca_free((void*)uri);
	uri = NULL;

	if(path[0] == '\0')
		strcpy(path, "/");

	const char getFmt[] =
		"GET %s HTTP/1.0\r\n"
		"Host: %s\r\n"	// Required for http 1.1
		"User-Agent: Rhinoca\r\n"
		"\r\n";

    int ret = 0;
	IPAddress adr;
	HttpStream* s = new HttpStream;

	if(uriLen > sizeof(s->getCmd) - sizeof(getFmt)) goto OnError;
	if(sprintf(s->getCmd, getFmt, path, host) < 0) goto OnError;

	s->buffer = NULL;
	s->headerSent = false;
	s->headerReceived = false;
	s->httpError = false;
	s->bufSize = 0;
	s->bufCapacity = 0;
	s->expectedTotalSize = 0;
	s->rh = rh;

	s->userReadCount = 0;

	// NOTE: Currently this host resolving operation is blocking
	if(!adr.parse(host))
		goto OnError;

	RHVERIFY(s->socket.create(BsdSocket::TCP) == 0);
	s->socket.setBlocking(false);

	ret = s->socket.connect(IPEndPoint(adr, 80));
	if(ret != 0 && !BsdSocket::inProgress(s->socket.lastError))
		goto OnError;

	return s;

OnError:
	print(rh, "Connection to %s failed\n", host);
	delete s;
	return NULL;
}

bool rhinoca_http_ready(void* file, rhuint64 size)
{
	RHASSERT(file);

    int ret = 0;
	HttpStream* s = reinterpret_cast<HttpStream*>(file);

	static const unsigned headerBufSize = 128;

	if(s->httpError)
		goto OnError;

	if(!s->headerSent) {
		ret = s->socket.send(s->getCmd, strlen(s->getCmd));
		if(ret < 0 && s->socket.lastError == ENOTCONN)
			return false;

		if(ret < 0 && !BsdSocket::inProgress(s->socket.lastError))
			goto OnError;

		s->headerSent = true;
	}

	if(!s->headerReceived) {
		prepareForRead(s, headerBufSize);

		ret = s->socket.receive(s->buffer + s->bufSize, headerBufSize);
		if(ret < 0 && BsdSocket::inProgress(s->socket.lastError))
			return false;
		if(ret == 0)
			goto OnEof;

		s->bufSize += ret;

		// Make sure the buffer are always null terminated
		s->buffer[s->bufSize] = '\0';

		// Check if header complete
		static const char headerSeperator[] = "\r\n\r\n";

		char* messageContent = strstr(s->buffer, headerSeperator);
		if(!messageContent)
			return false;

		messageContent += (sizeof(headerSeperator) - 1);	// -1 for the null terminator

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
			if(const char* p = strstr(s->buffer, "Content-Length:")) {
				if(sscanf(p, "Content-Length:%u", &s->expectedTotalSize) != 1)
					goto OnError;
			}
			else
				s->expectedTotalSize = unsigned(-1);

			unsigned bodySize = s->bufSize - (messageContent - s->buffer);
			// Move the received (if any) body content to the beginning of our buffer
			memmove(s->buffer, messageContent, bodySize);
			s->bufSize = bodySize;

			s->headerReceived = true;
		}
		else
		{
			print(s->rh, "Http stream receive server error code '%d'\n", serverRetCode);
			goto OnError;
		}
	}

	if(s->bufSize >= size)
		return true;

	RHASSERT(s->headerReceived);

	prepareForRead(s, (unsigned)size);
	ret = s->socket.receive(s->buffer + s->bufSize, (unsigned)size - s->bufSize);
	if(ret < 0 && BsdSocket::inProgress(s->socket.lastError))
		return false;
	if(ret < 0)
		goto OnError;
	if(ret == 0)
		goto OnEof;

	s->bufSize += ret;

	return s->bufSize >= size;

OnEof:
	return true;

OnError:
	print(s->rh, "read failed\n");
	s->httpError = true;
	return true;
}

rhuint64 rhinoca_http_read(void* file, void* buffer, rhuint64 size)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);

	float timeout = 3;
	while(!s->headerReceived || s->bufSize < size) {
		if(rhinoca_http_ready(file, size))
			break;
		TaskPool::sleep(0);

		// Detect timeout for connecting to remote host
		if(!s->headerReceived) {
			timeout -= s->timer.getDelta();
			if(timeout <= 0)
				s->httpError = true;
		}
	}

	if(!s->headerReceived) return 0;
	if(s->userReadCount >= s->expectedTotalSize) return 0;

	unsigned dataToMove = s->bufSize < (unsigned)size ? s->bufSize : (unsigned)size;

	// Copy s->buffer to destination
	memcpy(buffer, s->buffer, dataToMove);

	// Move data in s->buffer to the front
	memmove(s->buffer, s->buffer + dataToMove, s->bufSize - dataToMove);

	s->bufSize -= dataToMove;
	s->userReadCount += dataToMove;

//	printf("read: %u\n", s->userReadCount);

	return dataToMove;
}

void rhinoca_http_close(void* file)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	delete s;
}
