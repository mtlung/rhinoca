#include "pch.h"
#include "roImGui.h"
#include "../render/roCanvas.h"
#include "../render/roFont.h"

namespace ro {

namespace {

struct imGuiStates
{
	imGuiStates() : canvas(NULL) {}

	unsigned areaId;
	unsigned widgetId;
	Canvas* canvas;
};

}	// namespace

static imGuiStates _states;

void imGuiBegin(Canvas& canvas)
{
	roAssert(!_states.canvas);
	_states.canvas = &canvas;
	_states.canvas->save();
	_states.canvas->setTextBaseline("top");
}

void imGuiEnd()
{
	_states.canvas->restore();
	_states.canvas = NULL;
}

void imGuiSetTextAlign(const char* align)
{
	_states.canvas->setTextAlign(align);
}

void imGuiSetTextColor(float r, float g, float b, float a)
{
	_states.canvas->setGlobalColor(r, g, b, a);
}

static void _calTextRect(imGuiRect& rect, const roUtf8* str)
{
	TextMetrics metrics;
	_states.canvas->measureText(str, roSize(-1), metrics);

	rect.w = roMaxOf2(rect.w, int(metrics.width));
	rect.h = roMaxOf2(rect.h, int(metrics.height));
}

static void _calButtonRect(imGuiRect& rect, const roUtf8* str)
{
}

static void _placeRect(imGuiRect& rect)
{
}

// Draw functions

void _drawButton(imGuiRect rect, const roUtf8* text, bool enabled)
{
	Canvas& c = *_states.canvas;

	_calTextRect(rect, text);

	c.setGlobalColor(1, 1, 1, 1);
	c.setFillColor(0.8f, 0.8f, 0.8f, 1);
	c.fillRect(rect.x, rect.y, rect.w, rect.h);

	c.fillText(text, rect.x, rect.y, -1);
}

void imGuiLabel(imGuiRect rect, const roUtf8* text)
{
	_states.canvas->fillText(text, rect.x, rect.y, -1);
}

bool imGuiButton(imGuiRect rect, const roUtf8* text, bool enabled)
{
	_drawButton(rect, text, enabled);

	return false;
}


}	// namespace ro
