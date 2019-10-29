#include "pch.h"
#include "roSha1.h"
#include "roStringUtility.h"

namespace ro {

// Credit:
// https://github.com/deplinenoise/webby/blob/master/webby.c

Sha1::Sha1()
{
	state[0] = 0x67452301;
	state[1] = 0xefcdab89;
	state[2] = 0x98badcfe;
	state[3] = 0x10325476;
	state[4] = 0xc3d2e1f0;
	msg_size[0] = 0;
	msg_size[1] = 0;
	buf_used = 0;
}

static unsigned int sha1Rol(unsigned int value, unsigned int bits)
{
	return ((value) << bits) | (value >> (32 - bits));
}

static void sha1HashBlock(unsigned int state[5], const unsigned char* block)
{
	int i;
	unsigned int a, b, c, d, e;
	unsigned int w[80];

	// Prepare message schedule
	for (i = 0; i < 16; ++i)
		w[i] =
		(((unsigned int)block[(i * 4) + 0]) << 24) |
		(((unsigned int)block[(i * 4) + 1]) << 16) |
		(((unsigned int)block[(i * 4) + 2]) << 8) |
		(((unsigned int)block[(i * 4) + 3]) << 0);

	for (i = 16; i < 80; ++i)
		w[i] = sha1Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	// Initialize working variables
	a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

	// This is the core loop for each 20-word span.
	#define SHA1_LOOP(start, end, func, constant) \
	for (i = (start); i < (end); ++i) { \
	    unsigned int t = sha1Rol(a, 5) + (func) + e + (constant) + w[i]; \
		e = d; d = c; c = sha1Rol(b, 30); b = a; a = t; \
	}

	SHA1_LOOP(0,  20, ((b & c) ^ (~b & d)), 0x5a827999)
	SHA1_LOOP(20, 40, (b ^ c ^ d), 0x6ed9eba1)
	SHA1_LOOP(40, 60, ((b & c) ^ (b & d) ^ (c & d)), 0x8f1bbcdc)
	SHA1_LOOP(60, 80, (b ^ c ^ d), 0xca62c1d6)

#undef SHA1_LOOP

	// Update state
	state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}
void Sha1::update(const void* data_, size_t size)
{
	const char* data = (const char*)data_;
	unsigned int size_lo;
	unsigned int size_lo_orig;
	size_t remain = size;

	while (remain > 0)
	{
		size_t buf_space = (sizeof buffer) - buf_used;
		size_t copy_size = remain < buf_space ? remain : buf_space;
		memcpy(buffer + buf_used, data, copy_size);
		buf_used += copy_size;
		data += copy_size;
		remain -= copy_size;

		if (buf_used == sizeof buffer) {
			sha1HashBlock(state, buffer);
			buf_used = 0;
		}
	}

	size_lo = size_lo_orig = msg_size[1];
	size_lo += (unsigned int)(size * 8);

	if (size_lo < size_lo_orig)
		msg_size[0] += 1;

	msg_size[1] = size_lo;
}

void Sha1::update(const char* str)
{
	update(str, roStrLen(str));
}

void Sha1::final(unsigned char digest[20])
{
	unsigned char zero = 0x00;
	unsigned char one_bit = 0x80;
	unsigned char count_data[8];
	int i;

	// Generate size data in bit endian format
	for (i = 0; i < 8; ++i) {
		unsigned int word = msg_size[i >> 2];
		count_data[i] = (word >> ((3 - (i & 3)) * 8)) & 0xFF;
	}

	// Set trailing one-bit
	update(&one_bit, 1);

	// Emit null padding to to make room for 64 bits of size info in the last 512 bit block
	while (buf_used != 56)
		update(&zero, 1);

	// Write size in bits as last 64-bits
	update(count_data, 8);

	// Make sure we actually finalized our last block
	roAssert(0 == buf_used);

	// Generate digest
	for (i = 0; i < 20; ++i) {
		unsigned int word = state[i >> 2];
		unsigned char byte = (unsigned char)((word >> ((3 - (i & 3)) * 8)) & 0xff);
		digest[i] = byte;
	}
}

}	// namespace ro

constexpr size_t BASE64_QUADS_BEFORE_LINEBREAK = 19;

static size_t roBase64BufSize(size_t input_size)
{
	size_t triplets = (input_size + 2) / 3;
	size_t base_size = 4 * triplets;
	size_t line_breaks = 2 * (triplets / BASE64_QUADS_BEFORE_LINEBREAK);
	size_t null_termination = 1;
	return base_size + line_breaks + null_termination;
}

roStatus roBase64Encode(char* output, size_t output_size, const unsigned char* input, size_t input_size)
{
	static const char enc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
	size_t i = 0;
	int line_out = 0;

	if (output_size < roBase64BufSize(input_size))
		return roStatus::not_enough_buffer;

	while (i < input_size)
	{
		unsigned int idx_0, idx_1, idx_2, idx_3;
		unsigned int i0;

		i0 = (input[i]) << 16; i++;
		i0 |= (i < input_size ? input[i] : 0) << 8; i++;
		i0 |= (i < input_size ? input[i] : 0); i++;

		idx_0 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
		idx_1 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
		idx_2 = (i0 & 0xfc0000) >> 18; i0 <<= 6;
		idx_3 = (i0 & 0xfc0000) >> 18;

		if (i - 1 > input_size)
			idx_2 = 64;
		if (i > input_size)
			idx_3 = 64;

		*output++ = enc[idx_0];
		*output++ = enc[idx_1];
		*output++ = enc[idx_2];
		*output++ = enc[idx_3];

		if (++line_out == BASE64_QUADS_BEFORE_LINEBREAK) {
			*output++ = '\r';
			*output++ = '\n';
		}
	}

	*output = '\0';
	return roStatus::ok;
}
