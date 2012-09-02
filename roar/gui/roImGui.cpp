#include "pch.h"
#include "roImGui.h"
#include "../input/roInputDriver.h"
#include "../render/roCanvas.h"
#include "../render/roFont.h"
#include "../math/roVector.h"
#include "../roSubSystems.h"
#include <float.h>

namespace ro {

namespace {

static void _mergeRect(imGuiRect& rect1, const imGuiRect& rect2)
{
	float xmax = roMaxOf2(rect1.x + rect1.w, rect2.x + rect2.w);
	float ymax = roMaxOf2(rect1.y + rect1.h, rect2.y + rect2.h);
	rect1.x = roMinOf2(rect1.x, rect2.x);
	rect1.y = roMinOf2(rect1.y, rect2.y);
	rect1.w = xmax - rect1.x;
	rect1.h = ymax - rect1.y;
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
	StaticArray<TexturePtr, 6> texScrollPanel;
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
	texScrollPanel[1] = mgr->loadAs<Texture>("imGui/scrollbar-bar-bg.png");
	texScrollPanel[2] = mgr->loadAs<Texture>("imGui/scrollbar-bar-0.png");

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

struct PanelState
{
	imGuiRect rect;
	float* scollx;
	float* scolly;
	bool drawBorder;
};

struct imGuiStates
{
	imGuiStates() : canvas(NULL) {}

	unsigned areaId;
	unsigned widgetId;
	Canvas* canvas;

	float margin;
	float mousex, mousey;
	float mouseClickX, mouseClickY;
	bool mouseUp, mouseDown;

	Skin skin;

	Array<imGuiRect> rectStack;		// For deter
	Array<PanelState> panelStateStack;
};

}	// namespace

static imGuiStates _states;

roStatus imGuiInit()
{
	roStatus st;
	st = _states.skin.init(); if(!st) return st;

	imGuiSetMargin(5);

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

	_states.mousex = inputDriver->mouseAxis(inputDriver, stringHash("mouse x"));
	_states.mousey = inputDriver->mouseAxis(inputDriver, stringHash("mouse y"));
	_states.mouseUp = inputDriver->mouseButtonUp(inputDriver, 0, false);
	_states.mouseDown = inputDriver->mouseButtonDown(inputDriver, 0, false);

	if(_states.mouseDown) {
		_states.mouseClickX = _states.mousex;
		_states.mouseClickY = _states.mousey;
	}

	canvas.save();
	canvas.setTextAlign("center");
	canvas.setTextBaseline("middle");

	imGuiBeginScrollPanel(imGuiRect(0, 0, canvas.width(), canvas.height()), NULL, NULL, false);
}

void imGuiEnd()
{
	imGuiEndScrollPanel();

	_states.canvas->restore();
	_states.canvas = NULL;

	if(_states.mouseUp) {
		_states.mouseClickX = FLT_MAX;
		_states.mouseClickY = FLT_MAX;
	}
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

static bool _inRect(const imGuiRect& rect, float x, float y)
{
	return
		rect.x < x && x < rect.x + rect.w &&
		rect.y < y && y < rect.y + rect.h;
}

static bool _hasFocus(const imGuiRect& rect)
{
	return _inRect(rect, _states.mouseClickX, _states.mouseClickY);
}

static float _round(float x)
{
	return float(int(x));
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

bool imGuiButton(imGuiRect rect, const roUtf8* text, bool enabled)
{
	if(!text) text = "";

	imGuiRect textRect = _calTextRect(imGuiRect(rect.x, rect.y), text);
	rect = _calMarginRect(rect, textRect);
	_mergeRect(_states.rectStack.back(), rect);

	bool hover = _inRect(rect, _states.mousex, _states.mousey);
	bool focus = _hasFocus(rect);

	_drawButton(rect, text, enabled, hover, focus);

	return imGuiButtonLogic(rect);
}

bool imGuiButtonLogic(imGuiRect rect)
{
	bool hover = _inRect(rect, _states.mousex, _states.mousey);
	bool focus = _hasFocus(rect);

	return hover && focus && _states.mouseUp;
}

bool imGuiCheckBox(imGuiRect rect, const roUtf8* text, bool& state)
{
	if(!text) text = "";

	imGuiRect textRect = _calTextRect(imGuiRect(rect.x, rect.y), text);
	imGuiRect contentRect = textRect;
	contentRect.w += _states.skin.texCheckboxSize.x + _states.margin;
	contentRect.h = roMaxOf2(contentRect.h, _states.skin.texCheckboxSize.y);
	rect = _calMarginRect(rect, contentRect);
	_mergeRect(_states.rectStack.back(), rect);

	bool hover = _inRect(rect, _states.mousex, _states.mousey);
	bool focus = _hasFocus(rect);

	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	// Draw the box
	roRDriverTexture* tex = _states.skin.texCheckbox[(hover ? 2 : 0) + (state ? 1 : 0)]->handle;
	c.drawImage(
		tex, 0, 0, _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y,
		_round(rect.x + _states.margin), _round(rect.y + rect.h / 2 - _states.skin.texCheckboxSize.y / 2), _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y
	);

	// Draw the text
	float buttonDownOffset = focus ? 1.0f : 0;
	c.fillText(text, (rect.x + rect.w / 2) + _states.skin.texCheckboxSize.x / 2 + _states.margin / 2, rect.y + rect.h / 2 + buttonDownOffset, -1);

	bool clicked = imGuiButtonLogic(rect);
	if(clicked)
		state = !state;

	return clicked;
}

void imGuiBeginScrollPanel(imGuiRect rect, float* scollx, float* scolly, bool drawBorder)
{
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	PanelState state = { rect, scollx, scolly, drawBorder };
	_states.panelStateStack.pushBack(state);

	if(!_states.rectStack.isEmpty())
		_mergeRect(_states.rectStack.back(), rect);

	_states.rectStack.pushBack(rect);

	c.clipRect(rect.x, rect.y, rect.w, rect.h);
}

void imGuiEndScrollPanel()
{
	Canvas& c = *_states.canvas;
	const PanelState& panelState = _states.panelStateStack.back();

	c.resetClip();

	const imGuiRect& rect = panelState.rect;

	float border = 2;

	// Draw the border
	if(panelState.drawBorder)
		_draw3x3(_states.skin.texScrollPanel[0]->handle, panelState.rect, border, false);

	imGuiRect virtualRect = _states.rectStack.back();
	_mergeRect(virtualRect, panelState.rect);

	// Draw the scroll bar
	if(panelState.scolly)
	{
		// Background
		roRDriverTexture* tex = _states.skin.texScrollPanel[1]->handle;
		c.drawImage(tex,
			0, 0, tex->width, rect.h - border * 2,
			rect.x + rect.w - tex->width - border, rect.y + border, tex->width, rect.h - border * 2
		);

		// The bar
		float barHeight = panelState.rect.h * panelState.rect.h / virtualRect.h - border * 2;
		tex = _states.skin.texScrollPanel[2]->handle;

		imGuiRect barRect(
			rect.x + rect.w - tex->width - border,
			rect.y + border,
			tex->width,
			barHeight
		);
		_draw3x3(tex, barRect, 2);
	}

	_states.panelStateStack.popBack();
	_states.rectStack.popBack();
}

}	// namespace ro
