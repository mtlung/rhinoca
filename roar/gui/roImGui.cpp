#include "pch.h"
#include "roImGui.h"

namespace ro {

namespace {

struct imGuiStates
{
	unsigned areaId;
	unsigned widgetId;
};

}	// namespace

static imGuiStates _states;

void imGuiBegin(Canvas& canvas)
{
}

void imGuiEnd()
{
}

void imGuiLabel(const roUtf8* text)
{
}

bool imGuiButton(const roUtf8* text, bool enabled)
{
	return false;
}


}	// namespace ro
