#ifndef __render_roColor_h__
#define __render_roColor_h__

namespace ro {

class Colorf
{
public:
	/// Default color is black
	Colorf();

	Colorf(float r, float g, float b);
	Colorf(float r, float g, float b, float a);

// Operation
	/// http://msdn.microsoft.com/en-us/library/ms531197%28v=vs.85%29.aspx
	bool parse(const char* str);

	/// Str is of fixed format as "#rrggbbaa\0"
	void toString(char str[10]);

// Attributes
	union { struct {
		float r, g, b, a;
	};
		float data[4];
	};
};	// Colorf

}	// namespace ro

#endif	// __render_roColor_h__
