#include "pch.h"
#include "color.h"
#include "../common.h"
#include "../../roar/base/roStringUtility.h"
#include <stdio.h>

namespace Dom {

Color::Color()
	: r(0), g(0), b(0), a(1)
{
}

Color::Color(float r_, float g_, float b_)
	: r(r_), g(g_), b(b_), a(1)
{
}

Color::Color(float r_, float g_, float b_, float a_)
	: r(r_), g(g_), b(b_), a(a_)
{
}

// TODO: Parse more formats
bool Color::parse(const char* str)
{
	if(!str)
		return false;

	unsigned len = 0;
	char buf[512];

	const char* p = str;
	while(*p != '\0') {
		// Skip white space
		if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; continue; }

		buf[len] = (char)roToLower(*p);
		++len;
		if(len == COUNTOF(buf)) return false;
		++p;
	}
	buf[len] = '\0';

	{	// #RRGGBBAA
		int r_, g_, b_, a_;
		if(sscanf(buf, "#%2x%2x%2x%2x", &r_, &g_, &b_, &a_) == 4) {
			r = float(r_) / 255;
			g = float(g_) / 255;
			b = float(b_) / 255;
			a = float(a_) / 255;
			return true;
		}
	}

	{	// #RRGGBB
		int r_, g_, b_;
		if(sscanf(buf, "#%2x%2x%2x", &r_, &g_, &b_) == 3) {
			r = float(r_) / 255;
			g = float(g_) / 255;
			b = float(b_) / 255;
			a = 1;
			return true;
		}
	}

	{	// #RGBA
		int r_, g_, b_, a_;
		if(sscanf(buf, "#%1x%1x%1x%1x", &r_, &g_, &b_, &a_) == 4) {
			r = float(r_ * 0x10 + r_) / 255;
			g = float(g_ * 0x10 + g_) / 255;
			b = float(b_ * 0x10 + b_) / 255;
			a = float(a_ * 0x10 + a_) / 255;
			return true;
		}
	}

	{	// #RGB
		int r_, g_, b_;
		if(sscanf(buf, "#%1x%1x%1x", &r_, &g_, &b_) == 3) {
			r = float(r_ * 0x10 + r_) / 255;
			g = float(g_ * 0x10 + g_) / 255;
			b = float(b_ * 0x10 + b_) / 255;
			a = 1;
			return true;
		}
	}

	{	// rgba(255, 0, 0, 0.5)
		float r_, g_, b_, a_;
		if(sscanf(buf, "rgba(%f,%f,%f,%f)", &r_, &g_, &b_, &a_) == 4) {
			r = r_ / 255; g = g_ / 255; b = b_ / 255; a = a_;
			return true;
		}
	}

	{	// rgba(255, 0, 0, 255)
		float r_, g_, b_;
		if(sscanf(buf, "rgb(%f,%f,%f)", &r_, &g_, &b_) == 3) {
			r = r_ / 255; g = g_ / 255; b = b_ / 255; a = 1;
			return true;
		}
	}

	struct NamedColor {
		const char* name;
		Color color;
	};

	static const float x80 = float(0x80)/0xFF;
	static const float xC0 = float(0xC0)/0xFF;

	static const NamedColor namedColors[] = {
		{ "black",		Color(0, 0, 0) },
		{ "silver",		Color(xC0, xC0, xC0) },
		{ "gray",		Color(x80, x80, x80) },
		{ "white",		Color(1, 1, 1) },

		{ "maroon",		Color(x80, 0, 0) },
		{ "red",		Color(1, 0, 0) },
		{ "purple",		Color(x80, 0, x80) },
		{ "fuchsia",	Color(1, 0, 1) },

		{ "green",		Color(0, x80, 0) },
		{ "lime",		Color(0, 1, 0) },
		{ "ilive",		Color(x80, 0, x80) },
		{ "yellow",		Color(1, 1, 0) },

		{ "navy",		Color(0, 0, x80) },
		{ "blue",		Color(0, 0, 1) },
		{ "teal",		Color(0, x80, x80) },
		{ "aqua",		Color(0, 1, 1) },
	};

	for(unsigned i=0; i<COUNTOF(namedColors); ++i)
		if(roStrCaseCmp(buf, namedColors[i].name) == 0) {
			*this = namedColors[i].color;
			return true;
		}

	return false;
}

void Color::toString(char str[10])
{
	int r_ = int(r * 255);
	int g_ = int(g * 255);
	int b_ = int(b * 255);
	int a_ = int(a * 255);

	sprintf(str, "#%02x%02x%02x%02x", r_, g_, b_, a_);
}

}	// namespace Dom
