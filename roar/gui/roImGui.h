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

// States
struct imGuiWigetState {
	imGuiWigetState();
	imGuiRect rect;
	bool isEnable;
	bool isHover;
};

// Common
bool imGuiInRect(const imGuiRect& rect, float x, float y);

bool imGuiInClipRect(float x, float y);
void imGuiBeginClip(imGuiRect rect);	// Clip away any drawing and inputs outside this rect until imGuiEndClip() is called.
void imGuiEndClip();

// Label
void imGuiLabel(imGuiRect rect, const roUtf8* text);

// Check box
bool imGuiCheckBox(imGuiRect rect, const roUtf8* text, bool& state);

// Button
struct imGuiButtonState : public imGuiWigetState {
	imGuiButtonState();
};
bool imGuiButton(imGuiButtonState& state, const roUtf8* text);
bool imGuiButtonLogic(imGuiButtonState& state);

// Scroll bar
struct imGuiScrollBarState : public imGuiWigetState {
	imGuiScrollBarState();
	imGuiButtonState arrowButton1, arrowButton2;
	imGuiButtonState barButton;
	float pageSize;
	float value, valueMax;
	float smallStep, largeStep;
};
void imGuiVScrollBar(imGuiScrollBarState& state);
void imGuiHScrollBar(imGuiScrollBarState& state);
void imGuiVScrollBarLogic(imGuiScrollBarState& state);
void imGuiHScrollBarLogic(imGuiScrollBarState& state);

// Panel
struct imGuiPanelState : public imGuiWigetState {
	imGuiPanelState();
	bool showBorder;
	bool scrollable;
	imGuiScrollBarState hScrollBar, vScrollBar;
	imGuiRect _clientRect;
	imGuiRect _virtualRect;
};
void imGuiBeginScrollPanel(imGuiPanelState& state);
void imGuiEndScrollPanel();

// options
void imGuiSetMargin(float margin);
void imGuiSetTextAlign(const char* align);
void imGuiSetTextColor(float r, float g, float b, float a);

}	// namespace ro

#endif	// __gui_roImGui_h__
