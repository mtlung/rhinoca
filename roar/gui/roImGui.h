#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../platform/roCompiler.h"

namespace ro {

struct Canvas;

struct imGuiRect {
	imGuiRect() { x = y = w = h = 0; }
	imGuiRect(int x, int y) : x(x), y(y) { w = h = 0; }
	imGuiRect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
	int x, y;
	int w, h;
};

void imGuiBegin(Canvas& canvas);
void imGuiEnd();

// Widgets
void imGuiLabel(const imGuiRect& rect, const roUtf8* text);
bool imGuiButton(const imGuiRect& rect, const roUtf8* text, bool enabled=true);

// Text options
void imGuiSetTextAlign(const char* align);
void imGuiSetTextColor(float r, float g, float b, float a);

}	// namespace ro

#endif	// __gui_roImGui_h__
