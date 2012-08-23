#include "pch.h"
#include "roImGui.h"
#include "../render/roCanvas.h"

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

void imGuiLabel(const imGuiRect& rect, const roUtf8* text)
{
	_states.canvas->fillText(text, rect.x, rect.y, -1);
}

bool imGuiButton(const imGuiRect& rect, const roUtf8* text, bool enabled)
{
	_states.canvas->fillText(text, rect.x, rect.y, -1);

	return false;
}


}	// namespace ro
