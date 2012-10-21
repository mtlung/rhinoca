#include "pch.h"
#include "roGui.h"
#include "../base/roStopWatch.h"
#include "../base/roTypeCast.h"
#include "../input/roInputDriver.h"
#include "../render/roCanvas.h"
#include "../render/roFont.h"
#include "../math/roVector.h"
#include "../roSubSystems.h"
#include <float.h>

namespace ro {

namespace {

struct Skin
{
	roStatus init();
	void close();

	void tick();

// Attributes
	Vec2 texCheckboxSize;

	StaticArray<TexturePtr, 4> texCheckbox;
	StaticArray<TexturePtr, 9> texScrollPanel;
};

roStatus Skin::init()
{
	if(!roSubSystems) return roStatus::not_initialized;

	ResourceManager* mgr = roSubSystems->resourceMgr;
	if(!mgr) return roStatus::not_initialized;

	const Colorf white = Colorf(1, 1, 1);
	const Colorf transparent = Colorf(0, 0, 0, 0);

	GuiStyle transparentStyle;
	transparentStyle.border = 0;
	transparentStyle.padding = 5;
	transparentStyle.margin = 1;
	transparentStyle.normal.textColor = white;
	transparentStyle.hover.textColor = Colorf(0.7f, 0.9f, 0.9f);
	transparentStyle.active.textColor = transparentStyle.hover.textColor;
	transparentStyle.normal.backgroundColor = transparent;
	transparentStyle.hover.backgroundColor = transparent;
	transparentStyle.active.backgroundColor = transparent;

	GuiStyle opaqueStyle = transparentStyle;
	opaqueStyle.normal.backgroundColor = white;
	opaqueStyle.hover.backgroundColor = white;
	opaqueStyle.active.backgroundColor = white;

	{	GuiStyle& style = guiSkin.label;
		style = transparentStyle;
	}

	{	GuiStyle& style = guiSkin.checkBox;
		style = transparentStyle;
	}

	{	GuiStyle& style = guiSkin.button;
		style = opaqueStyle;
		style.border = 2;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/button.png");
		style.hover.backgroundImage = mgr->loadAs<Texture>("imGui/button_.png");
		style.active.backgroundImage = mgr->loadAs<Texture>("imGui/button_.png");
	}

	{	GuiStyle& style = guiSkin.vScrollbarUpButton;
		style = opaqueStyle;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-upbutton-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-upbutton-active.png");
	}

	{	GuiStyle& style = guiSkin.vScrollbarDownButton;
		style = opaqueStyle;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-downbutton-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-downbutton-active.png");
	}

	{	GuiStyle& style = guiSkin.vScrollbarThumbButton;
		style = opaqueStyle;
		style.border = 2;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/vscrollbar-thumb-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = style.normal.backgroundImage;
	}

	{	GuiStyle& style = guiSkin.hScrollbarLeftButton;
		style = opaqueStyle;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-leftbutton-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-leftbutton-active.png");
	}

	{	GuiStyle& style = guiSkin.hScrollbarRightButton;
		style = opaqueStyle;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-rightbutton-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = mgr->loadAs<Texture>("imGui/scrollbar-rightbutton-active.png");
	}

	{	GuiStyle& style = guiSkin.hScrollbarThumbButton;
		style = opaqueStyle;
		style.border = 2;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/hscrollbar-thumb-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = style.normal.backgroundImage;
	}

	{	GuiStyle& style = guiSkin.panel;
		style = opaqueStyle;
		style.border = 2;
		style.padding = 1;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/panel-normal.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = style.normal.backgroundImage;
	}

	{	GuiStyle& style = guiSkin.textArea;
		style = transparentStyle;
		style.border = 2;
	}

	{	GuiStyle& style = guiSkin.tabArea;
		style = opaqueStyle;
		style.border = 5;
		style.normal.backgroundImage = mgr->loadAs<Texture>("imGui/tabpanel.png");
		style.hover.backgroundImage = style.normal.backgroundImage;
		style.active.backgroundImage = style.normal.backgroundImage;
	}

	texCheckbox[0] = mgr->loadAs<Texture>("imGui/checkbox-0.png");
	texCheckbox[1] = mgr->loadAs<Texture>("imGui/checkbox-1.png");
	texCheckbox[2] = mgr->loadAs<Texture>("imGui/checkbox-2.png");
	texCheckbox[3] = mgr->loadAs<Texture>("imGui/checkbox-3.png");

	texScrollPanel.assign(NULL);
	texScrollPanel[0] = mgr->loadAs<Texture>("imGui/panel-normal.png");
	texScrollPanel[1] = mgr->loadAs<Texture>("imGui/vscrollbar-bar-bg.png");
	texScrollPanel[5] = mgr->loadAs<Texture>("imGui/hscrollbar-bar-bg.png");

	return roStatus::ok;
}

static const GuiStyle::StateSensitiveStyle& _selectStateSensitiveSytle(const GuiWigetState& state, const GuiStyle& style)
{
	const GuiStyle::StateSensitiveStyle* ret = &style.normal;
	if(state.isHover) ret = &style.hover;
	if(state.isActive) ret = &style.active;

	return *ret;
}

static roRDriverTexture* _selectBackgroundTexture(const GuiWigetState& state, const GuiStyle& style)
{
	const GuiStyle::StateSensitiveStyle& stateSensitiveSytle = _selectStateSensitiveSytle(state, style);
	return stateSensitiveSytle.backgroundImage ? stateSensitiveSytle.backgroundImage->handle : NULL;
}

static void _clearStyle(GuiStyle& style)
{
	style.normal.backgroundImage = NULL;
	style.hover.backgroundImage = NULL;
	style.active.backgroundImage = NULL;
}

void Skin::close()
{
	texCheckbox.assign(NULL);
	texScrollPanel.assign(NULL);
}

void Skin::tick()
{
	texCheckboxSize = texCheckbox[0] ?
		Vec2((float)texCheckbox[0]->width(), (float)texCheckbox[0]->height()) : Vec2(0.0f);
}

struct guiStates
{
	struct MouseState {
		float mousex, mousey, mousez;
		float mousedx, mousedy;
		float mouseClickx, mouseClicky;
		bool mouseUp, mouseDown;
	};

	guiStates();

	float mousex() { return mouseCaptured ? mouseCapturedState.mousex : mouseState.mousex + offsetx; }
	float mousey() { return mouseCaptured ? mouseCapturedState.mousey : mouseState.mousey + offsety; }
	float& mousez() { return mouseCaptured ? mouseCapturedState.mousey : mouseState.mousez; }
	float& mousedx() { return mouseCaptured ? mouseCapturedState.mousedx : mouseState.mousedx; }
	float& mousedy() { return mouseCaptured ? mouseCapturedState.mousedy : mouseState.mousedy; }
	float mouseClickx() { return mouseCaptured ? mouseCapturedState.mouseClickx : mouseState.mouseClickx + offsetx; }
	float mouseClicky() { return mouseCaptured ? mouseCapturedState.mouseClicky : mouseState.mouseClicky + offsety; }
	bool& mouseUp() { return mouseCaptured ? mouseCapturedState.mouseUp : mouseState.mouseUp; }
	bool& mouseDown() { return mouseCaptured ? mouseCapturedState.mouseDown : mouseState.mouseDown; }

	void setMousex(float v) { if(mouseCaptured) mouseCapturedState.mousex = v; else mouseState.mousex = v; }
	void setMousey(float v) { if(mouseCaptured) mouseCapturedState.mousey = v; else mouseState.mousey = v; }
	void setMouseClickx(float v) { if(mouseCaptured) mouseCapturedState.mouseClickx = v; else mouseState.mouseClickx = v; }
	void setMouseClicky(float v) { if(mouseCaptured) mouseCapturedState.mouseClicky = v; else mouseState.mouseClicky = v; }

	Canvas* canvas;

	bool mouseCaptured;
	MouseState mouseState;
	MouseState mouseCapturedState;
	float offsetx, offsety;

	bool mousePulse;
	PeriodicTimer mousePulseTimer;

	Skin skin;

	void* hotObject;
	void* lastFrameHotObject;
	void* potentialHotObject;
	void* hoveringObject;
	void* lastFrameHoveringObject;

	GuiPanelState rootPanel;

	Array<GuiWigetState*> groupStack;
	Array<GuiPanelState*> panelStateStack;
	Array<GuiWindowState*> windowList;
	Array<void*> ptrStack;
	Array<float> floatStack;
	Array<String> stringStack;
};

guiStates::guiStates()
{
	canvas = NULL;
	roZeroMemory(&mouseState, sizeof(mouseState));
	roZeroMemory(&mouseCapturedState, sizeof(mouseCapturedState));
	offsetx = offsety = 0;
	mousePulse = false;
	hoveringObject = NULL;
	hoveringObject = NULL;
	lastFrameHoveringObject = NULL;
}

}	// namespace

GuiSkin guiSkin;

static guiStates _states;

GuiStyle::GuiStyle()
{
	border = padding = margin = 0;
	normal.backgroundColor = hover.backgroundColor = active.backgroundColor = Colorf(0, 0);
}

roStatus guiInit()
{
	roStatus st;
	st = _states.skin.init(); if(!st) return st;

	_states.hotObject = NULL;
	_states.lastFrameHotObject = NULL;
	_states.potentialHotObject = NULL;
	_states.hoveringObject = NULL;
	_states.lastFrameHoveringObject = NULL;
	_states.mousePulseTimer.reset(0.1f);
	_states.mousePulseTimer.pause();

	return roStatus::ok;
}

void guiClose()
{
	_clearStyle(guiSkin.label);
	_clearStyle(guiSkin.checkBox);
	_clearStyle(guiSkin.button);
	_clearStyle(guiSkin.vScrollbarUpButton);
	_clearStyle(guiSkin.vScrollbarDownButton);
	_clearStyle(guiSkin.vScrollbarThumbButton);
	_clearStyle(guiSkin.hScrollbarLeftButton);
	_clearStyle(guiSkin.hScrollbarRightButton);
	_clearStyle(guiSkin.hScrollbarThumbButton);
	_clearStyle(guiSkin.panel);
	_clearStyle(guiSkin.textArea);
	_clearStyle(guiSkin.tabArea);
	_states.skin.close();
}

void guiBegin(Canvas& canvas)
{
	roAssert(!_states.canvas);
	_states.canvas = &canvas;

	_states.skin.tick();

	roInputDriver* inputDriver = roSubSystems->inputDriver;

	roAssert(!_states.mouseCaptured);
	_states.setMousex(inputDriver->mouseAxis(inputDriver, stringHash("mouse x")));
	_states.setMousey(inputDriver->mouseAxis(inputDriver, stringHash("mouse y")));
	_states.mousedx() = inputDriver->mouseAxisDelta(inputDriver, stringHash("mouse x"));
	_states.mousedy() = inputDriver->mouseAxisDelta(inputDriver, stringHash("mouse y"));
	_states.mousez() = inputDriver->mouseAxisDelta(inputDriver, stringHash("mouse z"));
	_states.mouseUp() = inputDriver->mouseButtonUp(inputDriver, 0, false);
	_states.mouseDown() = inputDriver->mouseButtonDown(inputDriver, 0, false);
	_states.offsetx = 0;
	_states.offsety = 0;

	_states.mousePulse = 
		_states.mousePulseTimer.isTriggered() &&
		_states.mousePulseTimer._stopWatch.getFloat() > 0.4f;

	if(_states.mouseDown()) {
		_states.mousePulseTimer.resume();
		_states.setMouseClickx(_states.mousex());
		_states.setMouseClicky(_states.mousey());
	}

	if(_states.mouseUp()) {
		_states.hotObject = NULL;
		_states.mousePulseTimer.reset();
		_states.mousePulseTimer.pause();
	}

	_states.potentialHotObject = NULL;

	canvas.setTextAlign("center");
	canvas.setTextBaseline("middle");

	_states.rootPanel.showBorder = false;
	_states.rootPanel.scrollable = false;
	_states.rootPanel.rect = Rectf(0, 0, (float)canvas.width(), (float)canvas.height());
	GuiStyle rootPanelStyle = guiSkin.panel;
	rootPanelStyle.padding = 0;
	guiBeginScrollPanel(_states.rootPanel, &rootPanelStyle);
}

static void _drawWindows();

void guiEnd()
{
	guiEndScrollPanel();

	_drawWindows();

	_states.canvas = NULL;

	_states.lastFrameHotObject = _states.hotObject;
	_states.lastFrameHoveringObject = _states.hoveringObject;

	if(!_states.hotObject)
		_states.hotObject = _states.potentialHotObject;

//	printf("%x\n", _states.hotObject);

	if(_states.mouseUp()) {
		_states.setMouseClickx(FLT_MAX);
		_states.setMouseClicky(FLT_MAX);
		_states.hotObject = NULL;
	}
}

void guiLayout(Rectf& rect)
{
}

static Sizef _calTextExtend(const roUtf8* str)
{
	if(!str || *str == '\0')
		return Sizef();

	TextMetrics metrics;
	_states.canvas->measureText(str, roSize(-1), FLT_MAX, metrics);

	return Sizef(metrics.width, metrics.height);
}

static void _mergeExtend(Rectf& rect1, const Rectf& rect2)
{
	rect1.w = roMaxOf2(rect1.w, rect2.x + rect2.w);
	rect1.h = roMaxOf2(rect1.h, rect2.y + rect2.h);
}

static void _setContentExtend(GuiWigetState& state, const GuiStyle& style, const Sizef& size)
{
	Rectf& deduced = state.deducedRect;

	// Add padding to the content rect
	deduced = state.rect;
	deduced.w = roMaxOf2(deduced.w, size.w + 2 * (style.padding + style.border));
	deduced.h = roMaxOf2(deduced.h, size.h + 2 * (style.padding + style.border));

	if(!_states.panelStateStack.isEmpty())
		_mergeExtend(_states.panelStateStack.back()->_virtualRect, state.deducedRect);
}

static bool _isHover(const Rectf& rect)
{
	float x = _states.mousex();
	float y = _states.mousey();
	return rect.isPointInRect(x, y) && guiInClipRect(x, y);
}

static bool _isHot(const Rectf& rect)
{
	float x = _states.mouseClickx();
	float y = _states.mouseClicky();
	return rect.isPointInRect(x, y) && guiInClipRect(x, y);
}

static void _updateWigetState(GuiWigetState& state)
{
	state.isHover = _isHover(state.deducedRect);
	state.isActive = _isHot(state.deducedRect);
	state.isClicked = state.isHover && state.isActive && _states.mouseUp();
	state.isClickRepeated = state.isActive && _states.mousePulse;

	if(state.isHover)
		_states.hoveringObject = &state;

	if(state.isActive)
		_states.potentialHotObject = &state;
}

static bool _isClicked(const Rectf& rect)
{
	return _isHover(rect) && _isHot(rect) && _states.mouseUp();
}

static bool _isRepeatedClick(const Rectf& rect)
{
	return _states.mousePulse && _isHot(rect);
}

static float _round(float x)
{
	return float(int(x));
}

bool guiInClipRect(float x, float y)
{
	// Convert to global coordinate
	x -= _states.offsetx;
	y -= _states.offsety;

	Rectf clipRect;
	_states.canvas->getClipRect((float*)&clipRect);

	return clipRect.isPointInRect(x, y);
}

void guiBeginClip(Rectf rect)
{
	Canvas& c = *_states.canvas;

	// Convert to global coordinate
	rect.x -= _states.offsetx;
	rect.y -= _states.offsety;

	c.save();
	c.clipRect(rect.x, rect.y, rect.w, rect.h);	// clipRect() will perform intersection
}

void guiEndClip()
{
	Canvas& c = *_states.canvas;
	c.restore();
}

void guiPushHostWiget(GuiWigetState& wiget)
{
	_states.groupStack.pushBack(&wiget);
}

GuiWigetState* guiGetHostWiget()
{
	if(_states.groupStack.isEmpty())
		return NULL;
	return _states.groupStack.back();
}

void guiPopHostWiget()
{
	if(!_states.groupStack.isEmpty())
		_states.groupStack.popBack();
}

void guiPushPtrToStack(void* ptr)
{
	_states.ptrStack.pushBack(ptr);
}

void* guiGetPtrFromStack(roSize index)
{
	if(_states.ptrStack.isEmpty())
		return NULL;
	return _states.ptrStack.back(index);
}

void guiPopPtrFromStack()
{
	if(!_states.ptrStack.isEmpty())
		_states.ptrStack.popBack();
}

void guiPushFloatToStack(float value)
{
	_states.floatStack.pushBack(value);
}

static float _dummyFloat = 0;
float& guiGetFloatFromStack(roSize index)
{
	if(_states.floatStack.isEmpty())
		return _dummyFloat;
	return _states.floatStack.back(index);
}

float guiPopFloatFromStack()
{
	float ret = guiGetFloatFromStack(0);
	_states.floatStack.popBack();
	return ret;
}

void guiPushStringToStack(const roUtf8* str)
{
	_states.stringStack.pushBack(str);
}

static String _dummyString;
String& guiGetStringFromStack(roSize index)
{
	if(_states.stringStack.isEmpty())
		return _dummyString;
	return _states.stringStack.back(index);
}

void guiPopStringFromStack()
{
	_states.stringStack.popBack();
}

typedef void (*LayoutCallback)(const Rectf& rect, Rectf& deducedRect, float margin);

void guiDoLayout(const Rectf& rect, Rectf& deducedRect, float margin)
{
	LayoutCallback callback = (LayoutCallback)guiGetPtrFromStack(0);
	if(callback)
		(*callback)(rect, deducedRect, margin);
}

// Draw functions

static void _draw3x3(roRDriverTexture* tex, const Rectf& rect, float borderWidth, bool drawBorder=true, bool drawMiddle=true)
{
	if(!tex) return;
	Canvas& c = *_states.canvas;

	float texw = (float)tex->width;
	float texh = (float)tex->height;
	float srcx[3] = { 0,		borderWidth,			texw - borderWidth			};	// From left to right
	float dstx[3] = { rect.x,	rect.x + borderWidth,	rect.right() - borderWidth	};	//
	float srcy[3] = { 0,		borderWidth,			texh - borderWidth			};	// From top to bottom
	float dsty[3] = { rect.y,	rect.y + borderWidth,	rect.bottom() - borderWidth	};	//
	float srcw[3] = { borderWidth,	texw - 2 * borderWidth,		borderWidth };
	float dstw[3] = { borderWidth,	rect.w - borderWidth * 2,	borderWidth };
	float srch[3] = { borderWidth,	texh - 2 * borderWidth,		borderWidth };
	float dsth[3] = { borderWidth,	rect.h - borderWidth * 2,	borderWidth };

	static const roSize ix_[8] = { 0, 1, 2, 0, 1, 2, 0, 2 };
	static const roSize iy_[8] = { 0, 0, 0, 2, 2, 2, 1, 1 };

	c.beginDrawImageBatch();

	if(drawBorder) for(roSize i=0; i<roCountof(ix_); ++i) {
		roSize ix = ix_[i];
		roSize iy = iy_[i];
		c.drawImage(tex, srcx[ix], srcy[iy], srcw[ix], srch[iy], dstx[ix], dsty[iy], dstw[ix], dsth[iy]);
	}

	float dstBorderWidth = drawBorder ? borderWidth : 0;
	if(drawMiddle) c.drawImage(
		tex,
		borderWidth, borderWidth, tex->width - borderWidth * 2, tex->height - borderWidth * 2,
		dstBorderWidth + rect.x, dstBorderWidth + rect.y, rect.w - dstBorderWidth * 2, rect.h - dstBorderWidth * 2
	);

	c.endDrawImageBatch();
}

void guiDrawBox(GuiWigetState& state, const roUtf8* text, const GuiStyle& style, bool drawBorder, bool drawCenter)
{
	const Rectf& rect = state.deducedRect;
	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, style);

	// Draw the background
	roRDriverTexture* tex = _selectBackgroundTexture(state, style);
	c.setGlobalColor(sStyle.backgroundColor.data);
	if(sStyle.backgroundColor.a > 0 && !tex) {
		c.setFillColor(1, 1, 1, 1);
		c.fillRect(rect.x, rect.y, rect.w, rect.h);
	}
	else if(tex) {
		c.setFillColor(0, 0, 0, 0);
		_draw3x3(tex, rect, style.border, drawBorder, drawCenter);
	}

	// Draw the text
	if(text && *text != '\0') {
		c.setGlobalColor(sStyle.textColor.data);
		float buttonDownOffset = state.isActive ? 1.0f : 0;
		c.fillText(text, rect.centerx(), rect.centery() + buttonDownOffset, -1);
	}
}

GuiWigetState::GuiWigetState()
{
	isEnable = false;
	isHover = false;
	isActive = false;
	isClicked = false;
	isClickRepeated = false;
	isLastFrameEnable = false;
	isLastFrameHover = false;
	isLastFrameActive = false;
}

#include "roGuiLabel.h"
#include "roGuiCheckBox.h"
#include "roGuiButton.h"
#include "roGuiScrollbar.h"
#include "roGuiPanel.h"
#include "roGuiWindow.h"
#include "roGuiTextArea.h"
#include "roGuiTabArea.h"
#include "roGuiLayout.h"

}	// namespace ro
