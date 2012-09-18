#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../math/roRect.h"
#include "../render/roColor.h"
#include "../render/roTexture.h"

namespace ro {

struct Canvas;

// Style
struct GuiStyle
{
	GuiStyle();

	struct StateSensitiveStyle
	{
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
struct GuiSkin
{
	GuiStyle label;
	GuiStyle button;
	GuiStyle checkBox;
	GuiStyle vScrollbar;
	GuiStyle vScrollbarUpButton;
	GuiStyle vScrollbarDownButton;
	GuiStyle vScrollbarThumbButton;
	GuiStyle hScrollbar;
	GuiStyle hScrollbarLeftButton;
	GuiStyle hScrollbarRightButton;
	GuiStyle hScrollbarThumbButton;
	GuiStyle panel;
	GuiStyle textArea;
	GuiStyle tabArea;
};

// The current skin to use
extern GuiSkin guiSkin;

// States
struct GuiWigetState
{
	GuiWigetState();

	bool isEnable;
	bool isHover;
	bool isActive;
	bool isClicked;
	bool isClickRepeated;
	bool isLastFrameEnable;
	bool isLastFrameHover;
	bool isLastFrameActive;
	Rectf rect;

	Rectf _deducedRect;	// The final rectangle after considered content size and layout engine
};

// Common
roStatus guiInit();
void guiClose();

void guiBegin(Canvas& canvas);
void guiEnd();

bool guiInClipRect(float x, float y);
void guiBeginClip(Rectf rect);	// Clip away any drawing and inputs outside this rect until guiEndClip() is called.
void guiEndClip();

void guiDrawBox(GuiWigetState& state, const roUtf8* text, const GuiStyle& style, bool drawBorder, bool drawCenter);

void guiPushHostWiget(GuiWigetState& wiget);	// For wiget grouping ie: guiBeginScrollPanel/guiEndScrollPanel
GuiWigetState* guiGetHostWiget();
void guiPopHostWiget();

void guiPushPtrToStack(void* ptr);
void* guiGetPtrFromStack(roSize index);
void guiPopPtrFromStack();

void guiPushFloatToStack(float value);
float& guiGetFloatFromStack(roSize index);
void guiPopFloatFromStack();

void guiDoLayout(Rectf& rect, float margin);

// Label
void guiLabel(const Rectf& rect, const roUtf8* text);

// Check box
bool guiCheckBox(const Rectf& rect, const roUtf8* text, bool& state);

// Button
struct GuiButtonState : public GuiWigetState {
};
bool guiButton(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);
void guiButtonDraw(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);
bool guiButtonLogic(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);

// Scroll bar
struct GuiScrollBarState : public GuiWigetState
{
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
struct GuiPanelState : public GuiWigetState
{
	GuiPanelState();
	bool showBorder;
	bool scrollable;
	GuiScrollBarState hScrollBar, vScrollBar;
	Rectf _clientRect;
	Rectf _virtualRect;
};
void guiBeginScrollPanel(GuiPanelState& state, const GuiStyle* style=NULL);
void guiEndScrollPanel();

// Text area
struct GuiTextAreaState : public GuiPanelState {
};
void guiTextArea(GuiTextAreaState& state, const roUtf8* text);

// Tab area
struct GuiTabAreaState : public GuiWigetState {
	GuiPanelState tabButtons;	// Panel to host the tab buttons
};
void guiBeginTabs(GuiTabAreaState& state);
void guiEndTabs();
bool guiBeginTab(const roUtf8* text);
void guiEndTab();

// Layout
void guiBeginFlowLayout(Rectf rect, char hORv);
void guiEndFlowLayout();

}	// namespace ro

#endif	// __gui_roImGui_h__
