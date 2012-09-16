#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../base/roStatus.h"
#include "../math/roRect.h"
#include "../render/roColor.h"
#include "../render/roTexture.h"

namespace ro {

struct Canvas;

roStatus imGuiInit();
void imGuiClose();

void imGuiBegin(Canvas& canvas);
void imGuiEnd();

// Style
struct imGuiStyle {
	Colorf backgroundColor;
	TexturePtr backgroundImage;
};

// States
struct imGuiWigetState {
	imGuiWigetState();
	Rectf rect;
	bool isEnable;
	bool isHover;
};

// Common
bool imGuiInClipRect(float x, float y);
void imGuiBeginClip(Rectf rect);	// Clip away any drawing and inputs outside this rect until imGuiEndClip() is called.
void imGuiEndClip();

// Label
void imGuiLabel(Rectf rect, const roUtf8* text);

// Check box
bool imGuiCheckBox(Rectf rect, const roUtf8* text, bool& state);

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
	float value, valueMax;
	float smallStep, largeStep;
	float _pageSize;
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
	Rectf _clientRect;
	Rectf _virtualRect;
};
void imGuiBeginScrollPanel(imGuiPanelState& state);
void imGuiEndScrollPanel();

// Text area
struct imGuiTextAreaState : public imGuiPanelState {
};
void imGuiTextArea(imGuiTextAreaState& state, const roUtf8* text);

// options
void imGuiSetMargin(float margin);
void imGuiSetTextAlign(const char* align);
void imGuiSetTextColor(float r, float g, float b, float a);

}	// namespace ro

#endif	// __gui_roImGui_h__
