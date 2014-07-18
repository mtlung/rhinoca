inline roSize roStrLen(const roUtf8* str)
{
	if(!str) return 0;
	roSize len = 0;
	for(; *str; ++str, ++len) {}
	return len;
}

inline roSize roStrLen(const roUtf16* str)
{
	if(!str) return 0;
	roSize len = 0;
	for(; *str; ++str, ++len) {}
	return len;
}

inline roSize roStrLen(const roUtf8* str, roSize strLenMax)
{
	if(!str) return 0;
	roSize len = 0;
	for(; *str && len < strLenMax; ++str, ++len) {}
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

inline int roStrnCmp(const char* s1, roSize n1, const char* s2, roSize n2)
{
	roSize n = n1 < n2 ? n1 : n2;
	for(roSize i=0; i<n; ++s1, ++s2, ++i) {
		if(*s1 != *s2) return (*s1-*s2);
		if(*s1 == 0 || *s2 == 0) break;
	}
	if(n1 == n2) return 0;
	return n1 < n2 ? -1 : 1;
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

inline int roStrnCaseCmp(const char* s1, const char* s2, roSize n)
{
	for(roSize i=0; i<n; ++s1, ++s2, ++i) {
		char c1 = roToLower(*s1);
		char c2 = roToLower(*s2);
		if(c1 != c2) return (c1 - c2);
		if(c1 == 0 || c2 == 0 ) break;
	}
	return 0;
}

inline int roStrnCaseCmp(const char* s1, roSize n1, const char* s2, roSize n2)
{
	roSize n = n1 < n2 ? n1 : n2;
	for(roSize i=0; i<n; ++s1, ++s2, ++i) {
		char c1 = roToLower(*s1);
		char c2 = roToLower(*s2);
		if(c1 != c2) return (c1 - c2);
		if(c1 == 0 || c2 == 0 ) break;
	}

	if(n1 == n2) return 0;
	return n1 < n2 ? -1 : 1;
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

inline char* roStrChrCase(char* sz, char ch)
{
	char c = roToLower(ch);
	for( ; *sz; ++sz) {
		if(roToLower(*sz) == c) return sz;
	}
	return NULL;
}

inline char* roStrrChrCase(char* sz, char ch)
{
	roSize len = roStrLen(sz);
	if(len == 0) return NULL;
	char* s = &sz[len - 1];
	char c = roToLower(ch);
	for( ; s > sz; --s) {
		if(roToLower(*s) == c) return s;
	}
	return NULL;
}

inline const char* roStrChr(const char* sz, char ch)
{
	return roStrChr((char*)sz, ch);
}

inline const char* roStrrChr(const char* sz, char ch)
{
	return roStrrChr((char*)sz, ch);
}

inline const char* roStrChrCase(const char* sz, char ch)
{
	return roStrChrCase((char*)sz, ch);
}

inline const char* roStrrChrCase(const char* sz, char ch)
{
	return roStrrChrCase((char*)sz, ch);
}

inline const char* roStrStr(const char* a, const char* b)
{
	return roStrStr((char*)a, b);
}

inline const char* roStrStr(const char* a, const char* aEnd, const char* b)
{
	return roStrStr((char*)a, aEnd, b);
}

inline const char* roStrnStr(const char* a, roSize aLen, const char* b)
{
	return roStrnStr((char*)a, aLen, b);
}

inline const char* roStrrStr(const char* a, const char* b)
{
	return roStrrStr((char*)a, b);
}

inline const char* roStrrnStr(const char* a, roSize aLen, const char* b)
{
	return roStrrnStr((char*)a, aLen, b);
}

inline const char* roStrStrCase(const char* a, const char* b)
{
	return roStrStrCase((char*)a, b);
}

inline const char* roStrStrCase(const char* a, const char* aEnd, const char* b)
{
	return roStrStrCase((char*)a, aEnd, b);
}

inline const char* roStrnStrCase(const char* a, roSize aLen, const char* b)
{
	return roStrnStrCase((char*)a, aLen, b);
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

inline bool roIsDigit(char ch)
{
	return (ch - '0' + 0u) <= 9u;
}

inline bool roIsAlpha(char ch)
{
	// | 32 is a simple way toLower, which is enough for the comparison
	return ((ch | 32) - 'a' + 0u) <= ('z' - 'a' + 0u);
}
