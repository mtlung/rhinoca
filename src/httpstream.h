#ifndef __HTTPSTREAM_H__
#define __HTTPSTREAM_H__

#include "rhinoca.h"

RHINOCA_API char* rhinoca_http_encodeUrl(const char* uri);	///< You need to free the returning string
RHINOCA_API char* rhinoca_http_decodeUrl(const char* uri);	///< You need to free the returning string

RHINOCA_API void* rhinoca_http_open(Rhinoca* rh, const char* uri);
RHINOCA_API bool rhinoca_http_ready(void* file, rhuint64 size);
RHINOCA_API rhuint64 rhinoca_http_read(void* file, void* buffer, rhuint64 size);
RHINOCA_API void rhinoca_http_close(void* file);

#endif	// __HTTPSTREAM_H__
