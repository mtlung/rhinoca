// Borrowed from http://code.google.com/p/stringencoders/source/browse/trunk/src/modp_ascii.c
inline char charTolower(char c)
{
	roUint32 eax = c;
	roUint32 ebx = (0x7f7f7f7fu & eax) + 0x25252525u;
	ebx = (0x7f7f7f7fu & ebx) + 0x1a1a1a1au;
	ebx = ((ebx & ~eax) >> 2)  & 0x20202020u;
	return (char)(eax + ebx);
}

inline char charToupper(char c)
{
	roUint32 eax = c;
	roUint32 ebx = (0x7f7f7f7fu & eax) + 0x05050505u;
	ebx = (0x7f7f7f7fu & ebx) + 0x1a1a1a1au;
	ebx = ((ebx & ~eax) >> 2)  & 0x20202020u;
	return (char)(eax - ebx);
}
