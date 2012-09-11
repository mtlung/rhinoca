#include "pch.h"
#include "roImGui.h"
#include "../base/roStopWatch.h"
#include "../input/roInputDriver.h"
#include "../render/roCanvas.h"
#include "../render/roFont.h"
#include "../math/roVector.h"
#include "../roSubSystems.h"
#include <float.h>

namespace ro {

namespace {

static void _mergeExtend(imGuiRect& rect1, const imGuiRect& rect2)
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

struct imGuiStates
{
	struct MouseState {
		float mousex, mousey, mousez;
		float mousedx, mousedy;
		float mouseClickx, mouseClicky;
		bool mouseUp, mouseDown;
	};

	imGuiStates();

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

	imGuiPanelState rootPanel;

	Array<imGuiPanelState*> panelStateStack;
};

imGuiStates::imGuiStates()
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

static imGuiStates _states;

roStatus imGuiInit()
{
	roStatus st;
	st = _states.skin.init(); if(!st) return st;

	imGuiSetMargin(5);

	_states.hotObject = NULL;
	_states.lastFrameHotObject = NULL;
	_states.potentialHotObject = NULL;
	_states.hoveringObject = NULL;
	_states.lastFrameHoveringObject = NULL;
	_states.mousePulseTimer.reset(0.1f);
	_states.mousePulseTimer.pause();

	return roStatus::ok;
}

void imGuiClose()
{
	_states.skin.close();
}

void imGuiBegin(Canvas& canvas)
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
	_states.rootPanel.rect = imGuiRect(0, 0, (float)canvas.width(), (float)canvas.height());
	imGuiBeginScrollPanel(_states.rootPanel);
}

void imGuiEnd()
{
	imGuiEndScrollPanel();

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

void imGuiLayout(imGuiRect& rect)
{
}

void imGuiSetMargin(float margin)
{
	_states.margin = margin;
}

void imGuiSetTextAlign(const char* align)
{
	_states.canvas->setTextAlign(align);
}

void imGuiSetTextColor(float r, float g, float b, float a)
{
	_states.canvas->setGlobalColor(r, g, b, a);
}

static imGuiRect _calTextRect(const imGuiRect& prefered, const roUtf8* str)
{
	TextMetrics metrics;
	_states.canvas->measureText(str, FLT_MAX, metrics);

	imGuiRect ret = prefered;
	ret.w = roMaxOf2(prefered.w, metrics.width);
	ret.h = roMaxOf2(prefered.h, metrics.height);

	return ret;
}

static imGuiRect _calMarginRect(const imGuiRect& prefered, const imGuiRect& content)
{
	imGuiRect ret = content;

	ret.w = roMaxOf2(prefered.w, content.w + 2 * _states.margin);
	ret.h = roMaxOf2(prefered.h, content.h + 2 * _states.margin);

	return ret;
}

static bool _isHover(const imGuiRect& rect)
{
	float x = _states.mousex();
	float y = _states.mousey();
	return imGuiInRect(rect, x, y) && imGuiInClipRect(x, y);
}

static bool _isHot(const imGuiRect& rect)
{
	float x = _states.mouseClickx();
	float y = _states.mouseClicky();
	return imGuiInRect(rect, x, y) && imGuiInClipRect(x, y);
}

static bool _isClicked(const imGuiRect& rect)
{
	return _isHover(rect) && _isHot(rect) && _states.mouseUp();
}

static bool _isRepeatedClick(const imGuiRect& rect)
{
	return _states.mousePulse && _isHot(rect);
}

static float _round(float x)
{
	return float(int(x));
}

bool imGuiInRect(const imGuiRect& rect, float x, float y)
{
	return
		rect.x < x && x < rect.x + rect.w &&
		rect.y < y && y < rect.y + rect.h;
}

bool imGuiInClipRect(float x, float y)
{
	// Convert to global coordinate
	x -= _states.offsetx;
	y -= _states.offsety;

	imGuiRect clipRect;
	_states.canvas->getClipRect((float*)&clipRect);

	return imGuiInRect(clipRect, x, y);
}

void imGuiBeginClip(imGuiRect rect)
{
	Canvas& c = *_states.canvas;

	// Convert to global coordinate
	rect.x -= _states.offsetx;
	rect.y -= _states.offsety;

	c.save();
	c.clipRect(rect.x, rect.y, rect.w, rect.h);	// clipRect() will perform intersection
}

void imGuiEndClip()
{
	Canvas& c = *_states.canvas;
	c.restore();
}

// Draw functions

void _draw3x3(roRDriverTexture* tex, const imGuiRect& rect, float borderWidth, bool drawMiddle = true)
{
	if(!tex) return;
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	float texw = (float)tex->width;
	float texh = (float)tex->height;
	float srcx[3] = { 0,		borderWidth,			texw - borderWidth				};	// From left to right
	float dstx[3] = { rect.x,	rect.x + borderWidth,	rect.x + rect.w - borderWidth	};	//
	float srcy[3] = { 0,		borderWidth,			texh - borderWidth				};	// From top to bottom
	float dsty[3] = { rect.y,	rect.y + borderWidth,	rect.y + rect.h - borderWidth	};	//
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

void _drawButton(const imGuiRect& rect, const roUtf8* text, bool enabled, bool hover, bool down)
{
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	roRDriverTexture* tex = _states.skin.texButton[hover ? 1 : 0]->handle;

	_draw3x3(tex, rect, 3);

	float buttonDownOffset = down ? 1.0f : 0;
	c.fillText(text, rect.x + rect.w / 2, rect.y + rect.h / 2 + buttonDownOffset, -1);
}

void imGuiLabel(imGuiRect rect, const roUtf8* text)
{
	if(!text) text = "";

	imGuiRect textRect = _calTextRect(imGuiRect(rect.x, rect.y), text);
	rect = _calMarginRect(rect, textRect);
	_states.canvas->fillText(text, rect.x + rect.w / 2, rect.y + rect.h / 2, -1);
}

bool imGuiCheckBox(imGuiRect rect, const roUtf8* text, bool& state)
{
	if(!text) text = "";

	imGuiRect textRect = _calTextRect(imGuiRect(rect.x, rect.y), text);
	imGuiRect contentRect = textRect;
	contentRect.w += _states.skin.texCheckboxSize.x + _states.margin;
	contentRect.h = roMaxOf2(contentRect.h, _states.skin.texCheckboxSize.y);
	rect = _calMarginRect(rect, contentRect);
	_mergeExtend(_states.panelStateStack.back()->_virtualRect, rect);

	bool hover = _isHover(rect);
	bool hot = _isHot(rect);

	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	// Draw the box
	roRDriverTexture* tex = _states.skin.texCheckbox[(hover ? 2 : 0) + (state ? 1 : 0)]->handle;
	c.drawImage(
		tex, 0, 0, _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y,
		_round(rect.x + _states.margin), _round(rect.y + rect.h / 2 - _states.skin.texCheckboxSize.y / 2), _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y
	);

	// Draw the text
	float buttonDownOffset = hot ? 1.0f : 0;
	c.fillText(text, (rect.x + rect.w / 2) + _states.skin.texCheckboxSize.x / 2 + _states.margin / 2, rect.y + rect.h / 2 + buttonDownOffset, -1);

	imGuiButtonState buttonState;
	buttonState.rect = rect;
	bool clicked = imGuiButtonLogic(buttonState);
	if(clicked)
		state = !state;

	return clicked;
}

bool imGuiButton(imGuiButtonState& state, const roUtf8* text)
{
	if(!text) text = "";

	imGuiRect rect = state.rect;
	imGuiRect textRect = _calTextRect(imGuiRect(rect.x, rect.y), text);
	rect = _calMarginRect(rect, textRect);
	_mergeExtend(_states.panelStateStack.back()->_virtualRect, rect);

	bool clicked = imGuiButtonLogic(state);

	_drawButton(rect, text, state.isEnable, state.isHover, _isHot(rect));

	return clicked;
}

bool imGuiButtonLogic(imGuiButtonState& state)
{
	state.isHover = _isHover(state.rect);
	bool hot = _isHot(state.rect);

	if(hot)
		_states.potentialHotObject = &state;

	return state.isHover && hot && _states.mouseUp();
}

imGuiScrollBarState::imGuiScrollBarState()
{
	pageSize = 0;
	value = 0;
	valueMax = 0;
	smallStep = 5.f;
	largeStep = smallStep * 5;
}

void imGuiVScrollBar(imGuiScrollBarState& state)
{
	imGuiVScrollBarLogic(state);

	Canvas& c = *_states.canvas;
	const imGuiRect& rect = state.rect;

	c.setGlobalColor(1, 1, 1, 1);

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[1]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, (float)texBg->width, rect.h,
		rect.x, rect.y, rect.w, rect.h
	);

	// The up button
	imGuiRect& rectBut1 = state.arrowButton1.rect;
	bool isUpButtonHot = _isHot(rectBut1);
	roRDriverTexture* texBut = _states.skin.texScrollPanel[isUpButtonHot ? 4 : 3]->handle;
	c.drawImage(
		texBut,
		0, 0, (float)texBut->width, texBut->height / 2.f,
		rectBut1.x, rectBut1.y, rectBut1.w, rectBut1.h
	);

	// The down button
	imGuiRect& rectBut2 = state.arrowButton2.rect;
	bool isDownButtonHot = _isHot(rectBut2);
	texBut = _states.skin.texScrollPanel[isDownButtonHot ? 4 : 3]->handle;
	c.drawImage(
		texBut,
		0, texBut->height / 2.f, (float)texBut->width, texBut->height / 2.f,
		rectBut2.x, rectBut2.y, rectBut2.w, rectBut2.h
	);

	// The bar
	roRDriverTexture* texBar = _states.skin.texScrollPanel[2]->handle;
	_draw3x3(texBar, state.barButton.rect, 2);
}

void imGuiHScrollBar(imGuiScrollBarState& state)
{
	imGuiHScrollBarLogic(state);

	Canvas& c = *_states.canvas;
	const imGuiRect& rect = state.rect;

	c.setGlobalColor(1, 1, 1, 1);

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[5]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, rect.w, (float)texBg->height,
		rect.x, rect.y, rect.w, rect.h
	);

	// The left button
	imGuiRect& rectBut1 = state.arrowButton1.rect;
	bool isLeftButtonHot = _isHot(rectBut1);
	roRDriverTexture* texBut = _states.skin.texScrollPanel[isLeftButtonHot ? 8 : 7]->handle;
	c.drawImage(
		texBut,
		0, 0, texBut->width / 2.f, (float)texBut->height,
		rectBut1.x, rectBut1.y, rectBut1.w, rectBut1.h
	);

	// The right button
	imGuiRect& rectBut2 = state.arrowButton2.rect;
	bool isRightButtonHot = _isHot(rectBut2);
	texBut = _states.skin.texScrollPanel[isRightButtonHot ? 8 : 7]->handle;
	c.drawImage(
		texBut,
		texBut->width / 2.f, 0, texBut->width / 2.f, (float)texBut->height,
		rectBut2.x, rectBut2.y, rectBut2.w, rectBut2.h
	);

	// The bar
	roRDriverTexture* texBar = _states.skin.texScrollPanel[6]->handle;
	_draw3x3(texBar, state.barButton.rect, 2);
}

void imGuiVScrollBarLogic(imGuiScrollBarState& state)
{
	const imGuiRect& rect = state.rect;
	imGuiRect& rectButU = state.arrowButton1.rect;
	imGuiRect& rectButD = state.arrowButton2.rect;
	float slideSize = rect.h - rectButU.h - rectButD.h;
	float barSize = roMaxOf2((state.pageSize * slideSize) / (state.valueMax + state.pageSize), 10.f);

	// Update buttons
	roRDriverTexture* texBut = _states.skin.texScrollPanel[3]->handle;

	rectButU = imGuiRect(rect.x, rect.y, rect.w, texBut->height / 2.f);
	state.arrowButton1.isHover = _isHover(rectButU);

	rectButD = imGuiRect(rect.x, rect.y + rect.h - texBut->height / 2.f, rect.w, texBut->height / 2.f);
	state.arrowButton2.isHover = _isHover(rectButD);

	// Handle arrow button click
	if(imGuiButtonLogic(state.arrowButton1) || _isRepeatedClick(rectButU))
		state.value -= state.smallStep;
	if(imGuiButtonLogic(state.arrowButton2) || _isRepeatedClick(rectButD))
		state.value += state.smallStep;

	// Handle bar background click
	imGuiRect rectBg(rect.x, rect.y + rectButU.h, rect.w, rect.h - rectButU.h - rectButD.h);
	if(_states.lastFrameHotObject != &state.barButton && (_isClicked(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClicky() < (rect.y + rect.h / 2))
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	imGuiButtonLogic(state.barButton);
	if(_states.hotObject == &state.barButton)
		state.value += (_states.mousedy() * state.valueMax / (slideSize - barSize));

	state.value = roClamp(state.value, 0.f, state.valueMax);

	// Update bar rect
    float barPos = ((slideSize - barSize) * state.value) / state.valueMax;
	state.barButton.rect = imGuiRect(
		rect.x,
		rect.y + rectButU.h + barPos,
		rect.w,
		barSize
	);
	state.barButton.isHover = _isHover(state.barButton.rect);
}

void imGuiHScrollBarLogic(imGuiScrollBarState& state)
{
	const imGuiRect& rect = state.rect;
	imGuiRect& rectButL = state.arrowButton1.rect;
	imGuiRect& rectButR = state.arrowButton2.rect;
	float slideSize = rect.w - rectButL.w - rectButR.w;
	float barSize = roMaxOf2((state.pageSize * slideSize) / (state.valueMax + state.pageSize), 10.f);

	// Update buttons
	roRDriverTexture* texBut = _states.skin.texScrollPanel[7]->handle;

	rectButL = imGuiRect(rect.x, rect.y, texBut->width / 2.f, rect.h);
	state.arrowButton1.isHover = _isHover(rectButL);

	rectButR = imGuiRect(rect.x + rect.w - texBut->width / 2.f, rect.y, texBut->width / 2.f, rect.h);
	state.arrowButton2.isHover = _isHover(rectButR);

	// Handle arrow button click
	if(imGuiButtonLogic(state.arrowButton1) || _isRepeatedClick(rectButL))
		state.value -= state.smallStep;
	if(imGuiButtonLogic(state.arrowButton2) || _isRepeatedClick(rectButR))
		state.value += state.smallStep;

	// Handle bar background click
	imGuiRect rectBg(rect.x + rectButL.w, rect.y, rect.w - rectButL.w - rectButR.w, rect.h);
	if(_states.lastFrameHotObject != &state.barButton && (_isClicked(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClickx() < (rect.x + rect.w / 2))
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	imGuiButtonLogic(state.barButton);
	if(_states.hotObject == &state.barButton)
		state.value += (_states.mousedx() * state.valueMax / (slideSize - barSize));

	state.value = roClamp(state.value, 0.f, state.valueMax);

	// Update bar rect
	float barPos = ((slideSize - barSize) * state.value) / state.valueMax;
	state.barButton.rect = imGuiRect(
		rect.x + rectButL.w + barPos,
		rect.y,
		barSize,
		rect.h
	);
	state.barButton.isHover = _isHover(state.barButton.rect);
}

imGuiWigetState::imGuiWigetState()
{
	isEnable = false;
	isHover = false;
}

imGuiButtonState::imGuiButtonState()
{
}

imGuiPanelState::imGuiPanelState()
{
	showBorder = true;
	scrollable = true;
}

void imGuiBeginScrollPanel(imGuiPanelState& state)
{
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	const imGuiRect& rect = state.rect;

	if(!_states.panelStateStack.isEmpty())
		_mergeExtend(_states.panelStateStack.back()->_virtualRect, rect);

	_states.panelStateStack.pushBack(&state);

	if(state._clientRect.w == 0 || state._clientRect.h == 0)
		state._clientRect = state.rect;

	state.isHover = _isHover(state.rect);
	if(state.isHover)
		_states.hoveringObject = &state;

	// Detect mouse scroll
	if(_states.lastFrameHoveringObject == &state && state.scrollable)
		state.vScrollBar.value -= _states.mousez() * 10;

	state.vScrollBar.value = roClamp(state.vScrollBar.value, 0.f, state.vScrollBar.valueMax);
	state.hScrollBar.value = roClamp(state.hScrollBar.value, 0.f, state.hScrollBar.valueMax);

	// Initialize the virtual bound
	imGuiRect& virtualRect = state._virtualRect;
	virtualRect.x = rect.x - state.hScrollBar.value;
	virtualRect.y = rect.y - state.vScrollBar.value;
	virtualRect.w = 0;
	virtualRect.h = 0;

	imGuiBeginClip(state._clientRect);

	// NOTE: Truncate float value to integer, so we will always have pixel perfect match
	c.translate(_round(virtualRect.x), _round(virtualRect.y));

	_states.offsetx -= virtualRect.x;
	_states.offsety -= virtualRect.y;
}

void imGuiEndScrollPanel()
{
	imGuiPanelState& panelState = *_states.panelStateStack.back();

	imGuiEndClip();

	_states.mouseCaptured = false;

	float border = 2;
	const imGuiRect& rect = panelState.rect;
	imGuiRect& virtualRect = panelState._virtualRect;

	_states.offsetx += virtualRect.x;
	_states.offsety += virtualRect.y;

	// Determine whether we need to show scroll bars
	bool showVScrollBar = false;
	bool showHScrollBar = false;
	float vScrollBarThickness = (float)_states.skin.texScrollPanel[2]->width();
	float hScrollBarThickness = (float)_states.skin.texScrollPanel[6]->height();
	if(virtualRect.w > rect.w || virtualRect.h > rect.h) {
		showVScrollBar = virtualRect.h > rect.h;
		showHScrollBar = virtualRect.w > (rect.w - vScrollBarThickness);
	}

	// Update the client area
	imGuiRect& clientRect = panelState._clientRect;
	clientRect = rect;
	clientRect.x += panelState.showBorder ? border : 0;
	clientRect.w -= panelState.showBorder ? border * 2 : 0;
	clientRect.y += panelState.showBorder ? border : 0;
	clientRect.h -= panelState.showBorder ? border * 2 : 0;
	clientRect.w -= showVScrollBar ? vScrollBarThickness : 0;
	clientRect.h -= showHScrollBar ? hScrollBarThickness : 0;

	// Draw the border
	if(panelState.showBorder)
		_draw3x3(_states.skin.texScrollPanel[0]->handle, panelState.rect, border, false);

	if(panelState.scrollable)
	{
		// Draw the vertical scroll bar
		panelState.vScrollBar.rect = imGuiRect(
			rect.x + rect.w - vScrollBarThickness - border,
			rect.y + border,
			vScrollBarThickness,
			rect.h - border * 2
		);
		panelState.vScrollBar.pageSize = (rect.h - border * 2) - (showHScrollBar ? hScrollBarThickness : 0);
		panelState.vScrollBar.valueMax = roMaxOf2(virtualRect.h - panelState.vScrollBar.pageSize, 0.f);
		if(showVScrollBar)
			imGuiVScrollBar(panelState.vScrollBar);

		// Draw the horizontal scroll bar
		panelState.hScrollBar.rect = imGuiRect(
			rect.x + border,
			rect.y + rect.h - hScrollBarThickness - border,
			rect.w - border * 2 - (showVScrollBar ? hScrollBarThickness : 0),
			hScrollBarThickness
		);
		panelState.hScrollBar.pageSize = (rect.w - border * 2) - (showVScrollBar ? vScrollBarThickness : 0);
		panelState.hScrollBar.valueMax = roMaxOf2(virtualRect.w - panelState.hScrollBar.pageSize, 0.f);
		if(showHScrollBar)
			imGuiHScrollBar(panelState.hScrollBar);
	}

	_states.panelStateStack.popBack();
}

}	// namespace ro
