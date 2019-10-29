#ifndef __roSha1_h__
#define __roSha1_h__

#include "roStatus.h"

namespace ro {

struct Sha1
{
	Sha1();
	void update(const void* data, size_t size);
	void update(const char* str);
	void final(unsigned char digest[20]);

protected:
	unsigned int state[5];
	unsigned int msg_size[2];
	size_t buf_used;
	unsigned char buffer[64];
};

}	// namespace ro

roStatus roBase64Encode(char* output, size_t output_size, const unsigned char* input, size_t input_size);

#endif	// __roSha1_h__
