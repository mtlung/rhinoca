#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../platform/roCompiler.h"

namespace ro {

struct Canvas;

void imGuiBegin(Canvas& canvas);
void imGuiEnd();

void imGuiLabel(const roUtf8* text);
bool imGuiButton(const roUtf8* text, bool enabled=true);

}	// namespace ro

#endif	// __gui_roImGui_h__
