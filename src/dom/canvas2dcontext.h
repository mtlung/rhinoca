#ifndef __DOME_CANVAS2DCONTEXT_H__
#define __DOME_CANVAS2DCONTEXT_H__

#include "canvas.h"

namespace Dom {

class Canvas2dContext : public Canvas::Context
{
public:
	explicit Canvas2dContext(Canvas*);
	~Canvas2dContext();

// Operations
	void bind(JSContext* cx, JSObject* parent);

// Attributes
	static JSClass jsClass;
};	// Canvas2dContext

}	// namespace Dom

#endif	// __DOME_CANVAS2DCONTEXT_H__
