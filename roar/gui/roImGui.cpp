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
}

void imGuiEnd()
{
	_states.canvas = NULL;
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
