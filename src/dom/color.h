#ifndef __DOM_COLOR_H__
#define __DOM_COLOR_H__

namespace Dom {

class Color
{
public:
	/// Default color is black
	Color();

	Color(float r, float g, float b);
	Color(float r, float g, float b, float a);

// Operation
	/// http://msdn.microsoft.com/en-us/library/ms531197%28v=vs.85%29.aspx
	bool parse(const char* str);

	/// Str is of fixed format as "#rrggbbaa\0"
	void toString(char str[10]);

// Attributes
	float r, g, b, a;
};	// Color

}	// namespace Dom

#endif	// __DOM_COLOR_H__
