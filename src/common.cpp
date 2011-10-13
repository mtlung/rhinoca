#include "pch.h"
#include "common.h"
#include "rhassert.h"
#include <ctype.h>
#include <math.h>
#include <assert.h>

#ifdef _MSC_VER
void rhAssert(const wchar_t* expression, const wchar_t* file, unsigned line)
{
	_wassert(expression, file, line);
}
#endif

// Reference: http://www.bsdlover.cn/study/UnixTree/V7/usr/src/libc/gen/atof.c.html
double atof(const char* p, double onErr)
{
	if(!p) return onErr;

	char c;
	double fl, flexp, exp5;
	double big = 72057594037927936.;  /*2^56*/
	const int LOGHUGE = 39;	// 10^39 --> Largest power
	int nd;
	char eexp, exp, neg, negexp, bexp;

	neg = 1;
	while((c = *p++) == ' ')
		;
	if (c == '-')
		neg = -1;
	else if (c=='+')
		;
	else
		--p;

	exp = 0;
	fl = 0;
	nd = 0;
	while ((c = *p++), isdigit(c)) {
		if (fl<big)
			fl = 10*fl + (c-'0');
		else
			exp++;
		nd++;
	}

	if (c == '.') {
		while ((c = *p++), isdigit(c)) {
			if (fl<big) {
				fl = 10*fl + (c-'0');
				exp--;
			}
		nd++;
		}
	}

	negexp = 1;
	eexp = 0;
	if ((c == 'E') || (c == 'e')) {
		if ((c= *p++) == '+')
			;
		else if (c=='-')
			negexp = -1;
		else
			--p;

		while ((c = *p++), isdigit(c)) {
			eexp = 10*eexp+(c-'0');
		}
		if (negexp<0)
			eexp = -eexp;
		exp = exp + eexp;
	}

	negexp = 1;
	if (exp<0) {
		negexp = -1;
		exp = -exp;
	}

	if((nd+exp*negexp) < -LOGHUGE){
		fl = 0;
		exp = 0;
	}
	flexp = 1;
	exp5 = 5;
	bexp = exp;
	for (;;) {
		if (exp&01)
			flexp *= exp5;
		exp >>= 1;
		if (exp==0)
			break;
		exp5 *= exp5;
	}
	if (negexp<0)
		fl /= flexp;
	else
		fl *= flexp;
	fl = ldexp(fl, negexp*bexp);
	if (neg<0)
		fl = -fl;
	return fl;
}

#if defined(_MSC_VER)
int strcasecmp(const char* s1, const char* s2)
{
	return _stricmp(s1, s2);
}
#endif
