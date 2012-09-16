#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../base/roStatus.h"
#include "../math/roRect.h"
#include "../render/roColor.h"
#include "../render/roTexture.h"

namespace ro {

struct Canvas;

// Style
struct GuiStyle {
	GuiStyle();

	struct StateSensitiveStyle {
		Colorf textColor;
		Colorf backgroundColor;
		TexturePtr backgroundImage;
	};

	StateSensitiveStyle normal;
	StateSensitiveStyle hover;
	StateSensitiveStyle active;

	float border;	// Number of pixels on each side of the background image that are not affected by the scale of the Control' shape
	float padding;	// Space in pixels from each edge of the Control to the start of its contents
	float margin;	// The margins between elements rendered in this style and any other GUI Controls
};

// Modeled over Unity Gui skin
// http://docs.unity3d.com/Documentation/Components/class-GUISkin.html
struct GuiSkin {
	GuiStyle button;
	GuiStyle vScrollbar;
	GuiStyle vScrollbarLeftButton;
	GuiStyle vScrollbarRightButton;
	GuiStyle hScrollbar;
	GuiStyle hScrollbarLeftButton;
	GuiStyle hScrollbarRightButton;
};

// The current skin to use
extern GuiSkin guiSkin;

// States
struct GuiWigetState {
	GuiWigetState();
	Rectf rect;
	bool isEnable;
	bool isHover;
};

// Common
roStatus guiInit();
void guiClose();

void guiBegin(Canvas& canvas);
void guiEnd();

bool guiInClipRect(float x, float y);
void guiBeginClip(Rectf rect);	// Clip away any drawing and inputs outside this rect until guiEndClip() is called.
void guiEndClip();

// Label
void guiLabel(Rectf rect, const roUtf8* text);

// Check box
bool guiCheckBox(Rectf rect, const roUtf8* text, bool& state);

// Button
struct GuiButtonState : public GuiWigetState {
	GuiButtonState();
};
bool guiButton(GuiButtonState& state, const roUtf8* text, const GuiStyle* style=NULL);
bool guiButtonLogic(GuiButtonState& state);

// Scroll bar
struct GuiScrollBarState : public GuiWigetState {
	GuiScrollBarState();
	GuiButtonState arrowButton1, arrowButton2;
	GuiButtonState barButton;
	float value, valueMax;
	float smallStep, largeStep;
	float _pageSize;
};
void guiVScrollBar(GuiScrollBarState& state);
void guiHScrollBar(GuiScrollBarState& state);
void guiVScrollBarLogic(GuiScrollBarState& state);
void guiHScrollBarLogic(GuiScrollBarState& state);

// Panel
struct GuiPanelState : public GuiWigetState {
	GuiPanelState();
	bool showBorder;
	bool scrollable;
	GuiScrollBarState hScrollBar, vScrollBar;
	Rectf _clientRect;
	Rectf _virtualRect;
};
void guiBeginScrollPanel(GuiPanelState& state);
void guiEndScrollPanel();

// Text area
struct GuiTextAreaState : public GuiPanelState {
};
void guiTextArea(GuiTextAreaState& state, const roUtf8* text);

// options
void guiSetMargin(float margin);
void guiSetTextAlign(const char* align);
void guiSetTextColor(float r, float g, float b, float a);

}	// namespace ro

#endif	// __gui_roImGui_h__
