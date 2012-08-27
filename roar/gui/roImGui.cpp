#include "pch.h"
#include "roImGui.h"
#include "../input/roInputDriver.h"
#include "../render/roCanvas.h"
//#include "../render/roFont.h"
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
	Vec2 texButtonSize;
	Vec2 texCheckboxSize;

	TexturePtr texButton[2];
	TexturePtr texCheckbox[2];
};

roStatus Skin::init()
{
	if(!roSubSystems) return roStatus::not_initialized;

	ResourceManager* mgr = roSubSystems->resourceMgr;
	if(!mgr) return roStatus::not_initialized;

	texButton[0] = mgr->loadAs<Texture>("imGui/button.png");
	texButton[1] = mgr->loadAs<Texture>("imGui/button_.png");

	texCheckbox[0] = mgr->loadAs<Texture>("imGui/checkbox.png");
	texCheckbox[1] = mgr->loadAs<Texture>("imGui/checkbox_.png");

	return roStatus::ok;
}

void Skin::close()
{
	texButton[0] = texButton[1] = NULL;
	texCheckbox[0] = texCheckbox[1] = NULL;
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
	imGuiStates() : canvas(NULL) {}

	unsigned areaId;
	unsigned widgetId;
	Canvas* canvas;

	float margin;
	float mousex, mousey;
	float mouseClickX, mouseClickY;
	bool mouseUp, mouseDown;

	Skin skin;
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
}

void imGuiEnd()
{
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

void _drawButton(const imGuiRect& rect, const roUtf8* text, bool enabled, bool hover, bool down)
{
	Canvas& c = *_states.canvas;

	float skin = 3;	// Spacing in the skin. TODO: Load from data

	c.setGlobalColor(1, 1, 1, 1);

	roRDriverTexture* tex = _states.skin.texButton[hover ? 1 : 0]->handle;

	c.beginDrawImageBatch();

	float srcx, srcy, srcw, srch;
	float dstx, dsty, dstw, dsth;

	// Left-top corner
	srcx = srcy = 0;
	srcw = srch = skin;
	dstx = rect.x; dsty = rect.y;
	dstw = dsth = skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Left side
	srcy = skin;
	srch = 15;
	dsty += skin;
	dsth = rect.h - 2 * skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Left-bottom corner
	srcy = 20;
	srch = skin;
	dsty = rect.y + rect.h - skin;
	dsth = skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Right-top corner
	srcx = 6; srcy = 0;
	srcw = srch = skin;
	dstx = rect.x + rect.w - skin; dsty = rect.y;
	dstw = dsth = skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Right side
	srcy = skin;
	srch = 15;
	dsty += skin;
	dsth = rect.h - 2 * skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Right-bottom corner
	srcy = 20;
	srch = skin;
	dsty = rect.y + rect.h - skin;
	dsth = skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Top side
	srcx = skin; srcy = 0;
	srcw = skin; srch = skin;
	dstx = rect.x + skin; dsty = rect.y;
	dstw = rect.w - 2 * skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Bottom side
	srcy = 20;
	dsty = rect.y + rect.h - skin;
	c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);

	// Middle part
	c.drawImage(
		tex, skin, skin, skin, 15,
		skin + rect.x, skin + rect.y, rect.w - skin * 2, rect.h - skin * 2
	);

	c.endDrawImageBatch();

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

	Canvas& c = *_states.canvas;

	float skin = 3;	// Spacing in the skin. TODO: Load from data
	c.setGlobalColor(1, 1, 1, 1);

	// Draw the box
	roRDriverTexture* tex = _states.skin.texCheckbox[state ? 1 : 0]->handle;
	c.drawImage(
		tex, 0, 0, _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y,
		_round(rect.x + _states.margin), _round(rect.y + rect.h / 2 - _states.skin.texCheckboxSize.y / 2), _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y
	);

	// Draw the text
	bool focus = _hasFocus(rect);
	float buttonDownOffset = focus ? 1.0f : 0;
	c.fillText(text, (rect.x + rect.w / 2) + _states.skin.texCheckboxSize.x / 2 + _states.margin / 2, rect.y + rect.h / 2 + buttonDownOffset, -1);

	bool clicked = imGuiButtonLogic(rect);
	if(clicked)
		state = !state;

	return clicked;
}

}	// namespace ro
