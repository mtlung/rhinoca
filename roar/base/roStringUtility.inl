inline roSize roStrLen(const char* str)
{
	if(!str) return 0;
	roSize len = 0;
	for(; *str; ++str, ++len) {}
	return len;
}

inline char* roStrnCpy(char* dst, const char* src, roSize n)
{
	char* ret = dst;
	for(roSize i=0; i<n; ++i) {
		if(*src == 0) break;
		*dst = *src;
		++dst; ++src;
	}
	*dst = 0;
	return ret;
}

inline int roStrCmp(const char* s1, const char* s2)
{
	for( ;; ++s1, ++s2) {
		if(*s1 != *s2) return (*s1-*s2);
		if(*s1 == 0 || *s2 == 0) break;
	}
	return 0;
}

inline int roStrnCmp(const char* s1, const char* s2, roSize n)
{
	for(roSize i=0; i<n; ++s1, ++s2, ++i) {
		if(*s1 != *s2) return (*s1-*s2);
		if(*s1 == 0 || *s2 == 0) break;
	}
	return 0;
}

inline int roStrCaseCmp(const char* s1, const char* s2)
{
	for( ;; ++s1, ++s2 ) {
		char c1 = roToLower(*s1);
		char c2 = roToLower(*s2);
		if(c1 != c2) return (c1 - c2);
		if(c1 == 0 || c2 == 0 ) break;
	}
	return 0;
}

inline char* roStrChr(char* sz, char ch)
{
	for( ; *sz; ++sz) {
		if(*sz == ch) return sz;
	}
	return NULL;
}

inline char* roStrrChr(char* sz, char ch)
{
	roSize len = roStrLen(sz);
	if(len == 0) return NULL;
	char* s = &sz[len - 1];
	for( ; s > sz; --s) {
		if(*s == ch) return s;
	}
	return NULL;
}

inline const char* roStrChr(const char* sz, char ch)
{
	return roStrChr((char*)sz, ch);
}

inline const char* roStrStr(const char* a, const char* b)
{
	return roStrStr((char*)a, b);
}

// Borrowed from http://code.google.com/p/stringencoders/source/browse/trunk/src/modp_ascii.c
inline char roToLower(char c)
{
	roUint32 eax = c;
	roUint32 ebx = (0x7f7f7f7fu & eax) + 0x25252525u;
	ebx = (0x7f7f7f7fu & ebx) + 0x1a1a1a1au;
	ebx = ((ebx & ~eax) >> 2)  & 0x20202020u;
	return (char)(eax + ebx);
}

inline char roToUpper(char c)
{
	roUint32 eax = c;
	roUint32 ebx = (0x7f7f7f7fu & eax) + 0x05050505u;
	ebx = (0x7f7f7f7fu & ebx) + 0x1a1a1a1au;
	ebx = ((ebx & ~eax) >> 2)  & 0x20202020u;
	return (char)(eax - ebx);
}
