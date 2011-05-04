#include "pch.h"
#include "httpstream.h"
#include "socket.h"
#include "taskpool.h"
#include "timer.h"

#include <string.h>

/// Notes on http 1.0 protocol:
/// Http 1.0 protocol contains an optional "Content-Length" attribute, but
/// if it's not present, the end of the data will indicated by a gracefull disconnection.
/// See: http://www.xml.com/pub/a/ws/2003/11/25/protocols.html
/// HTTP Make Really Easy
/// http://www.jmarshall.com/easy/http/
/// A list of HTTP client library
/// http://curl.haxx.se/libcurl/competitors.html
struct HttpStream
{
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
};

static void prepareForRead(HttpStream* s, unsigned readSize)
{
	int reserveForNull = 1;
	if(s->bufCapacity <= s->bufSize + readSize + reserveForNull) {
		s->buffer = (char*)rhinoca_realloc(s->buffer, s->bufCapacity, s->bufSize + readSize + reserveForNull);
		s->bufCapacity = s->bufSize + readSize + reserveForNull;
	}
}

void* rhinoca_http_open(Rhinoca* rh, const char* uri, int threadId)
{
	// Parse http://host
	char host[128] = "";
	char path[256] = "";
	sscanf(uri, "http://%[^/]%s", host, path);

	if(path[0] == '\0')
		strcpy(path, "/");

	const char getFmt[] =
		"GET %s HTTP/1.0\r\n"
		"User_Agent: HTTPTool/1.0\r\n"
		"\r\n";

    int ret = 0;
	IPAddress adr;
	HttpStream* s = new HttpStream;

	if(strlen(uri) > sizeof(s->getCmd) - sizeof(getFmt)) goto OnError;
	if(sprintf(s->getCmd, getFmt, path) < 0) goto OnError;

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

	VERIFY(s->socket.create(BsdSocket::TCP) == 0);
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

bool rhinoca_http_ready(void* file, rhuint64 size, int threadId)
{
	ASSERT(file);

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
		s->headerReceived = true;

		// Nullify the seperator between header and body
		messageContent[-1] = '\0';

		// Check for status
		int serverRetCode = 0;
		if(sscanf(s->buffer, "HTTP/%*c.%*c %d %*[^\n]", &serverRetCode) != 1)
			goto OnError;

		if( serverRetCode == 302 ||	// Found
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
			// Move the received (if any) body content to the begining of our buffer
			memmove(s->buffer, messageContent, bodySize);
			s->bufSize = bodySize;

//			printf("header received\n");
		}
	}

	if(s->bufSize >= size)
		return true;

	ASSERT(s->headerReceived);

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

rhuint64 rhinoca_http_read(void* file, void* buffer, rhuint64 size, int threadId)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);

	float timeout = 3;
	DeltaTimer timer;
	while(!s->headerReceived || s->bufSize < size) {
		if(rhinoca_http_ready(file, size, threadId))
			break;
		TaskPool::sleep(0);
		timeout -= timer.getDelta();

		if(timeout <= 0)
			s->httpError = true;
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

	printf("readed: %u\n", s->userReadCount);

	return dataToMove;
}

void rhinoca_http_close(void* file, int threadId)
{
	HttpStream* s = reinterpret_cast<HttpStream*>(file);
	delete s;
}
