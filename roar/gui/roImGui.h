#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../base/roStatus.h"

namespace ro {

struct Canvas;

struct imGuiRect {
	imGuiRect() { x = y = w = h = 0; }
	imGuiRect(float x, float y) : x(x), y(y) { w = h = 0; }
	imGuiRect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
	float x, y;
	float w, h;
};

roStatus imGuiInit();
void imGuiClose();

void imGuiBegin(Canvas& canvas);
void imGuiEnd();


// Widgets
void imGuiLabel(imGuiRect rect, const roUtf8* text);
bool imGuiButton(imGuiRect rect, const roUtf8* text, bool enabled=true);
bool imGuiButtonLogic(imGuiRect rect);
bool imGuiCheckBox(imGuiRect rect, const roUtf8* text, bool& state);

void imGuiBeginScrollPanel(imGuiRect rect, float* scollx, float* scolly, bool drawBorder=true);
void imGuiEndScrollPanel();

// options
void imGuiSetMargin(float margin);
void imGuiSetTextAlign(const char* align);
void imGuiSetTextColor(float r, float g, float b, float a);

}	// namespace ro

#endif	// __gui_roImGui_h__
