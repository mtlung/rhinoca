#include "pch.h"
#include "color.h"
#include "../common.h"

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
	unsigned len = 0;
	char buf[32];

	const char* p = str;
	while(*p != '\0') {
		if(*p == ' ') { ++p; continue; }	// Skip all space
		buf[len] = (char)tolower(*p);
		++len;
		if(len == COUNTOF(buf)) return false;
		++p;
	}
	buf[len] = '\0';

	int scanfRet = 0;

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
		if(sscanf(buf, "#%1x%1x%1x", &r_, &g_, &b_, &a_) == 4) {
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

	{	// rgba(255, 0, 0, 255)
		float r_, g_, b_, a_;
		if(sscanf(buf, "rgba(%f,%f,%f,%f)", &r_, &g_, &b_, &a_) == 4) {
			r = r_ / 255; g = g_ / 255; b = b_ / 255; a = a_ / 255;
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

	return false;
}

}	// namespace Dom
