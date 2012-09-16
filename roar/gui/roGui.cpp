#include "pch.h"
#include "roGui.h"
#include "../base/roStopWatch.h"
#include "../input/roInputDriver.h"
#include "../render/roCanvas.h"
#include "../render/roFont.h"
#include "../math/roVector.h"
#include "../roSubSystems.h"
#include <float.h>

namespace ro {

namespace {

static void _mergeExtend(Rectf& rect1, const Rectf& rect2)
{
	rect1.w = roMaxOf2(rect1.w, rect2.x + rect2.w);
	rect1.h = roMaxOf2(rect1.h, rect2.y + rect2.h);
}

struct Skin
{
	roStatus init();
	void close();

	void tick();

// Attributes
	Vec2 texButtonSize;
	Vec2 texCheckboxSize;

	StaticArray<TexturePtr, 2> texButton;
	StaticArray<TexturePtr, 4> texCheckbox;
	StaticArray<TexturePtr, 9> texScrollPanel;
};

roStatus Skin::init()
{
	if(!roSubSystems) return roStatus::not_initialized;

	ResourceManager* mgr = roSubSystems->resourceMgr;
	if(!mgr) return roStatus::not_initialized;

	guiSkin.button.normal.backgroundImage = mgr->loadAs<Texture>("imGui/button.png");
	guiSkin.button.hover.backgroundImage = mgr->loadAs<Texture>("imGui/button_.png");

	texButton[0] = mgr->loadAs<Texture>("imGui/button.png");
	texButton[1] = mgr->loadAs<Texture>("imGui/button_.png");

	texCheckbox[0] = mgr->loadAs<Texture>("imGui/checkbox-0.png");
	texCheckbox[1] = mgr->loadAs<Texture>("imGui/checkbox-1.png");
	texCheckbox[2] = mgr->loadAs<Texture>("imGui/checkbox-2.png");
	texCheckbox[3] = mgr->loadAs<Texture>("imGui/checkbox-3.png");

	texScrollPanel.assign(NULL);
	texScrollPanel[0] = mgr->loadAs<Texture>("imGui/panel-border-0.png");
	texScrollPanel[1] = mgr->loadAs<Texture>("imGui/vscrollbar-bar-bg.png");
	texScrollPanel[2] = mgr->loadAs<Texture>("imGui/vscrollbar-bar-0.png");
	texScrollPanel[3] = mgr->loadAs<Texture>("imGui/vscrollbar-arrow-0.png");
	texScrollPanel[4] = mgr->loadAs<Texture>("imGui/vscrollbar-arrow-1.png");
	texScrollPanel[5] = mgr->loadAs<Texture>("imGui/hscrollbar-bar-bg.png");
	texScrollPanel[6] = mgr->loadAs<Texture>("imGui/hscrollbar-bar-0.png");
	texScrollPanel[7] = mgr->loadAs<Texture>("imGui/hscrollbar-arrow-0.png");
	texScrollPanel[8] = mgr->loadAs<Texture>("imGui/hscrollbar-arrow-1.png");

	return roStatus::ok;
}

static void _clearStyle(GuiStyle& style)
{
	style.normal.backgroundImage = NULL;
	style.hover.backgroundImage = NULL;
	style.active.backgroundImage = NULL;
}

void Skin::close()
{
	texButton.assign(NULL);
	texCheckbox.assign(NULL);
	texScrollPanel.assign(NULL);
}

void Skin::tick()
{
	texButtonSize = texButton[0] ?
		Vec2((float)texButton[0]->width(), (float)texButton[0]->height()) : Vec2(0.0f);

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

	float margin;

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

	Array<GuiPanelState*> panelStateStack;
};

guiStates::guiStates()
{
	canvas = NULL;
	margin = 0;
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
//	textColor = Colorf(1, 1, 1);
}

roStatus guiInit()
{
	roStatus st;
	st = _states.skin.init(); if(!st) return st;

	guiSetMargin(5);

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
	_clearStyle(guiSkin.button);
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
	guiBeginScrollPanel(_states.rootPanel);
}

void guiEnd()
{
	guiEndScrollPanel();

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

void guiSetMargin(float margin)
{
	_states.margin = margin;
}

void guiSetTextAlign(const char* align)
{
	_states.canvas->setTextAlign(align);
}

void guiSetTextColor(float r, float g, float b, float a)
{
	_states.canvas->setGlobalColor(r, g, b, a);
}

static Rectf _calTextRect(const Rectf& prefered, const roUtf8* str)
{
	TextMetrics metrics;
	_states.canvas->measureText(str, FLT_MAX, metrics);

	Rectf ret = prefered;
	ret.w = roMaxOf2(prefered.w, metrics.width);
	ret.h = roMaxOf2(prefered.h, metrics.height);

	return ret;
}

static Rectf _calMarginRect(const Rectf& prefered, const Rectf& content)
{
	Rectf ret = content;

	ret.w = roMaxOf2(prefered.w, content.w + 2 * _states.margin);
	ret.h = roMaxOf2(prefered.h, content.h + 2 * _states.margin);

	return ret;
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

// Draw functions

void _draw3x3(roRDriverTexture* tex, const Rectf& rect, float borderWidth, bool drawMiddle = true)
{
	if(!tex) return;
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	float texw = (float)tex->width;
	float texh = (float)tex->height;
	float srcx[3] = { 0,		borderWidth,			texw - borderWidth				};	// From left to right
	float dstx[3] = { rect.x,	rect.x + borderWidth,	rect.right() - borderWidth	};	//
	float srcy[3] = { 0,		borderWidth,			texh - borderWidth				};	// From top to bottom
	float dsty[3] = { rect.y,	rect.y + borderWidth,	rect.bottom() - borderWidth	};	//
	float srcw[3] = { borderWidth,	texw - 2 * borderWidth,		borderWidth };
	float dstw[3] = { borderWidth,	rect.w - borderWidth * 2,	borderWidth };
	float srch[3] = { borderWidth,	texh - 2 * borderWidth,		borderWidth };
	float dsth[3] = { borderWidth,	rect.h - borderWidth * 2,	borderWidth };

	static const roSize ix_[8] = { 0, 1, 2, 0, 1, 2, 0, 2 };
	static const roSize iy_[8] = { 0, 0, 0, 2, 2, 2, 1, 1 };

	c.beginDrawImageBatch();

	for(roSize i=0; i<roCountof(ix_); ++i) {
		roSize ix = ix_[i];
		roSize iy = iy_[i];
		c.drawImage(tex, srcx[ix], srcy[iy], srcw[ix], srch[iy], dstx[ix], dsty[iy], dstw[ix], dsth[iy]);
	}

	if(drawMiddle) c.drawImage(
		tex,
		borderWidth, borderWidth, tex->width - borderWidth * 2, tex->height - borderWidth * 2,
		borderWidth + rect.x, borderWidth + rect.y, rect.w - borderWidth * 2, rect.h - borderWidth * 2
	);

	c.endDrawImageBatch();
}

GuiWigetState::GuiWigetState()
{
	isEnable = false;
	isHover = false;
}

void guiLabel(Rectf rect, const roUtf8* text)
{
	if(!text) text = "";

	Rectf textRect = _calTextRect(Rectf(rect.x, rect.y), text);
	rect = _calMarginRect(rect, textRect);
	_states.canvas->fillText(text, rect.centerx(), rect.centery(), -1);
}

#include "roGuiCheckBox.h"
#include "roGuiButton.h"
#include "roGuiScrollbar.h"
#include "roGuiPanel.h"
#include "roGuiTextArea.h"

}	// namespace ro
