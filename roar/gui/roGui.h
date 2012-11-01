#ifndef __gui_roImGui_h__
#define __gui_roImGui_h__

#include "../math/roRect.h"
#include "../math/roVector.h"
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
		Array<TexturePtr> auxImages;
	};

	StateSensitiveStyle normal;
	StateSensitiveStyle hover;
	StateSensitiveStyle active;

	float border;	// Number of pixels on each side of the background image that are not affected by the scale of the Control' shape
	float padding;	// Space in pixels from each edge of the Control to the start of its contents
	float margin;	// The margins between elements rendered in this style and any other GUI Controls
	bool tileCenter;
};

// Modeled over Unity Gui skin
// http://docs.unity3d.com/Documentation/Components/class-GUISkin.html
struct GuiSkin
{
	GuiStyle label;
	GuiStyle button;
//	GuiStyle toggleOn, toggleOff;
	GuiStyle checkBox;
	GuiStyle vScrollbarBG;
	GuiStyle vScrollbarUpButton;
	GuiStyle vScrollbarDownButton;
	GuiStyle vScrollbarThumbButton;
	GuiStyle hScrollbarBG;
	GuiStyle hScrollbarLeftButton;
	GuiStyle hScrollbarRightButton;
	GuiStyle hScrollbarThumbButton;
	GuiStyle panel;
	GuiStyle textArea;
	GuiStyle tabArea;
	GuiStyle window;
	GuiStyle windowTitle;
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
	Rectf deducedRect;	// The final rectangle after considered content size and layout engine
};

// Common
roStatus		guiInit					();
void			guiClose				();

void			guiBegin				(Canvas& canvas);
void			guiEnd					();

ro::Vec2		guiMousePos				();
ro::Vec2		guiMouseDownPos			();
ro::Vec2		guiMouseDragOffset		();

bool			guiInClipRect			(float x, float y);

void			guiDrawBox				(GuiWigetState& state, const roUtf8* text, const GuiStyle& style, bool drawBorder, bool drawCenter);

void			guiPushHostWiget		(GuiWigetState& wiget);	// For wiget grouping ie: guiBeginScrollPanel/guiEndScrollPanel
GuiWigetState*	guiGetHostWiget			();
void			guiPopHostWiget			();

void			guiPushPtrToStack		(void* ptr);
void*			guiGetPtrFromStack		(roSize index);
void			guiPopPtrFromStack		();

void			guiPushFloatToStack		(float value);
float&			guiGetFloatFromStack	(roSize index);
float			guiPopFloatFromStack	();

void			guiPushStringToStack	(const roUtf8* str);
String&			guiGetStringFromStack	(roSize index);
void			guiPopStringFromStack	();

void			guiDoLayout				(const Rectf& rect, Rectf& deducedRect, float margin);

// Label
void			guiLabel				(const Rectf& rect, const roUtf8* text);

// Check box
bool			guiCheckBox				(const Rectf& rect, const roUtf8* text, bool& state);

// Button
struct GuiButtonState : public GuiWigetState {
};
bool			guiButton				(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);
bool			guiToggleButton			(GuiButtonState& state, bool& onOffState, const roUtf8* text=NULL, const GuiStyle* onStyle=NULL, const GuiStyle* offStyle=NULL);
void			guiButtonDraw			(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);
bool			guiButtonLogic			(GuiButtonState& state, const roUtf8* text=NULL, const GuiStyle* style=NULL);

// Scroll bar
struct GuiScrollBarState : public GuiWigetState
{
	GuiScrollBarState();
	GuiButtonState arrowButton1, arrowButton2;
	GuiButtonState bgButton1, bgButton2;	// The background space between the bar and the arrows
	GuiButtonState barButton;
	float value, valueMax;
	float smallStep, largeStep;
	float _pageSize;
};
void			guiVScrollBar			(GuiScrollBarState& state);
void			guiHScrollBar			(GuiScrollBarState& state);
void			guiVScrollBarLogic		(GuiScrollBarState& state);
void			guiHScrollBarLogic		(GuiScrollBarState& state);

struct GuiWigetContainer : public GuiWigetState
{
	GuiWigetContainer();
	Rectf clientRect;
	Rectf virtualRect;	// Relative to deducedRect
	mutable Vec2 mouseClickPos;	// Each container store it's own relative position for the mouse click
};
void			guiBeginContainer		(GuiWigetContainer& container);
void			guiEndContainer			();

// Panel
struct GuiPanelState : public GuiWigetContainer
{
	GuiPanelState();
	bool showBorder;
	bool scrollable;
	GuiScrollBarState hScrollBar, vScrollBar;
};
void			guiBeginScrollPanel		(GuiPanelState& state, const GuiStyle* style=NULL);
void			guiEndScrollPanel		();

// Window
struct GuiWindowState : public GuiWigetContainer
{
	GuiWindowState();

	String title;
	GuiButtonState titleBar;	// We represent the title using a button

	void* userData;
	typedef void (*WindowFunction)(GuiWindowState& state);
	WindowFunction windowFunction;

	int _useCount;
};
void			guiWindow				(GuiWindowState& state, const GuiStyle* style=NULL);
void			guiFocusWindow			(GuiWindowState& state);
bool			guiIsInActiveWindow		();
bool			guiIsObstructedByOtherWindow(float x, float y);

// Text area
struct GuiTextAreaState : public GuiPanelState {
	GuiTextAreaState();
	roSize highLightBegPos;	// Means how many character before this position
	roSize highLightEndPos;	// It also means the caret position
};
void			guiTextArea				(GuiTextAreaState& state, String& text);

// Tab area
struct GuiTabAreaState : public GuiWigetState {
	GuiTabAreaState();
	GuiPanelState tabButtons;	// Panel to host the tab buttons
	GuiPanelState clientPanel;
	roSize activeTabIndex;
	roSize _tabCount;
};
void			guiBeginTabs			(GuiTabAreaState& state);
void			guiEndTabs				();
bool			guiBeginTab				(const roUtf8* text);
void			guiEndTab				();

// Layout
void			guiBeginFlowLayout		(Rectf rect, char hORv);
void			guiEndFlowLayout		();

}	// namespace ro

#endif	// __gui_roImGui_h__
