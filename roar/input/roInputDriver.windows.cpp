#include "pch.h"
#include "roInputDriver.h"
#include "../base/roArray.h"
#include "../base/roStringHash.h"
#include "../math/roVector.h"
#include "../platform/roPlatformHeaders.h"

using namespace ro;

static DefaultAllocator _allocator;

struct roInputDriverImpl : public roInputDriver
{
	struct WinEvent {
		HWND hWnd;
		UINT uMsg;
		WPARAM wParam;
		LPARAM lParam;
	};

	void _processEvents(void** data, roSize numData);
	void _processButtons(const WinEvent& e);

	void _popWinEvents();

	void _tick();

	Array<WinEvent> _winEvents;
	Array<ro::StringHash> _keyList, _keyDownList, _keyUpList;

	roUint8 _mouseButtonDownBits;	// 8 bit can store 8 mouse buttons
	roUint8 _mouseButtonUpBits;
	roUint8 _previousMouseButtonDownBits;
	roUint8 _previousMouseButtonUpBits;

	Vec2 _mousePos;
	Vec3 _mouseAxis, _mouseAxisRaw;
	Vec3 _previousMouseAxis, _previousMouseAxisRaw;
};

struct ButtonMapping {
	StringHash hash;
	unsigned vkCode;
	bool shift;
};

// http://docs.unity3d.com/Documentation/ScriptReference/KeyCode.html
static ButtonMapping _buttonMapping[] = {
	{ StringHash("Left"),		VK_LEFT,		false },
	{ StringHash("Right"),		VK_RIGHT,		false },
	{ StringHash("Up"),			VK_UP,			false },
	{ StringHash("Down"),		VK_DOWN,		false },
	{ StringHash("Escape"),		VK_ESCAPE,		false },
	{ StringHash("Space"),		VK_SPACE,		false },
	{ StringHash("Return"),		VK_RETURN,		false },
	{ StringHash("LAlt"),		VK_MENU,		false },
	{ StringHash("RAlt"),		VK_MENU,		false },
	{ StringHash("LControl"),	VK_CONTROL,		false },
	{ StringHash("RControl"),	VK_CONTROL,		false },
	{ StringHash("Numpad0"),	VK_NUMPAD0,		false },
	{ StringHash("Numpad1"),	VK_NUMPAD1,		false },
	{ StringHash("Numpad2"),	VK_NUMPAD2,		false },
	{ StringHash("Numpad3"),	VK_NUMPAD3,		false },
	{ StringHash("Numpad4"),	VK_NUMPAD4,		false },
	{ StringHash("Numpad5"),	VK_NUMPAD5,		false },
	{ StringHash("Numpad6"),	VK_NUMPAD6,		false },
	{ StringHash("Numpad7"),	VK_NUMPAD7,		false },
	{ StringHash("Numpad8"),	VK_NUMPAD8,		false },
	{ StringHash("Numpad9"),	VK_NUMPAD9,		false },
	{ StringHash("a"),			'A',			false },
	{ StringHash("b"),			'B',			false },
	{ StringHash("c"),			'C',			false },
	{ StringHash("d"),			'D',			false },
	{ StringHash("e"),			'E',			false },
	{ StringHash("f"),			'F',			false },
	{ StringHash("g"),			'G',			false },
	{ StringHash("h"),			'H',			false },
	{ StringHash("i"),			'I',			false },
	{ StringHash("j"),			'J',			false },
	{ StringHash("k"),			'K',			false },
	{ StringHash("l"),			'L',			false },
	{ StringHash("m"),			'M',			false },
	{ StringHash("n"),			'N',			false },
	{ StringHash("o"),			'O',			false },
	{ StringHash("p"),			'P',			false },
	{ StringHash("q"),			'Q',			false },
	{ StringHash("r"),			'R',			false },
	{ StringHash("s"),			'S',			false },
	{ StringHash("t"),			'T',			false },
	{ StringHash("u"),			'U',			false },
	{ StringHash("v"),			'V',			false },
	{ StringHash("w"),			'W',			false },
	{ StringHash("x"),			'X',			false },
	{ StringHash("y"),			'Y',			false },
	{ StringHash("z"),			'Z',			false },
	{ StringHash("0"),			'0',			false },
	{ StringHash("1"),			'1',			false },
	{ StringHash("2"),			'2',			false },
	{ StringHash("3"),			'3',			false },
	{ StringHash("4"),			'4',			false },
	{ StringHash("5"),			'5',			false },
	{ StringHash("6"),			'6',			false },
	{ StringHash("7"),			'7',			false },
	{ StringHash("8"),			'8',			false },
	{ StringHash("9"),			'9',			false },
	{ StringHash(";"),			VK_OEM_1,		false },
	{ StringHash("/"),			VK_OEM_2,		false },
	{ StringHash("`"),			VK_OEM_3,		false },
	{ StringHash("["),			VK_OEM_4,		false },
	{ StringHash("\\"),			VK_OEM_5,		false },
	{ StringHash("]"),			VK_OEM_6,		false },
	{ StringHash("'"),			VK_OEM_7,		false },
	{ StringHash("+"),			VK_OEM_PLUS,	false },
	{ StringHash("-"),			VK_OEM_MINUS,	false },
	{ StringHash(","),			VK_OEM_COMMA,	false },
	{ StringHash("."),			VK_OEM_PERIOD,	false },
	{ StringHash("Back"),		VK_BACK,		false },
	{ StringHash("Tab"),		VK_TAB,			false },
	{ StringHash("PageUp"),		VK_PRIOR,		false },
	{ StringHash("PageDown"),	VK_NEXT,		false },
	{ StringHash("End"),		VK_END,			false },
	{ StringHash("Home"),		VK_HOME,		false },
	{ StringHash("Insert"),		VK_INSERT,		false },
	{ StringHash("Delete"),		VK_DELETE,		false },
	{ StringHash("Add"),		VK_ADD,			false },
	{ StringHash("Subtract"),	VK_SUBTRACT,	false },
	{ StringHash("Multiply"),	VK_MULTIPLY,	false },
	{ StringHash("Divide"),		VK_DIVIDE,		false },
	{ StringHash("Pause"),		VK_PAUSE,		false },
	{ StringHash("F1"),			VK_F1,			false },
	{ StringHash("F2"),			VK_F2,			false },
	{ StringHash("F3"),			VK_F3,			false },
	{ StringHash("F4"),			VK_F4,			false },
	{ StringHash("F5"),			VK_F5,			false },
	{ StringHash("F6"),			VK_F6,			false },
	{ StringHash("F7"),			VK_F7,			false },
	{ StringHash("F8"),			VK_F8,			false },
	{ StringHash("F9"),			VK_F9,			false },
	{ StringHash("F10"),		VK_F10,			false },
	{ StringHash("F11"),		VK_F11,			false },
	{ StringHash("F12"),		VK_F12,			false },
	{ StringHash("F13"),		VK_F13,			false },
	{ StringHash("F14"),		VK_F14,			false },
	{ StringHash("F15"),		VK_F15,			false },
	{ StringHash("LSystem"),	VK_LWIN,		false },
	{ StringHash("RSystem"),	VK_RWIN,		false },
	{ StringHash("Menu"),		VK_APPS,		false },
	{ StringHash("A"),			'A',			true },
	{ StringHash("B"),			'B',			true },
	{ StringHash("C"),			'C',			true },
	{ StringHash("D"),			'D',			true },
	{ StringHash("E"),			'E',			true },
	{ StringHash("F"),			'F',			true },
	{ StringHash("G"),			'G',			true },
	{ StringHash("H"),			'H',			true },
	{ StringHash("I"),			'I',			true },
	{ StringHash("J"),			'J',			true },
	{ StringHash("K"),			'K',			true },
	{ StringHash("L"),			'L',			true },
	{ StringHash("M"),			'M',			true },
	{ StringHash("N"),			'N',			true },
	{ StringHash("O"),			'O',			true },
	{ StringHash("P"),			'P',			true },
	{ StringHash("Q"),			'Q',			true },
	{ StringHash("R"),			'R',			true },
	{ StringHash("S"),			'S',			true },
	{ StringHash("T"),			'T',			true },
	{ StringHash("U"),			'U',			true },
	{ StringHash("V"),			'V',			true },
	{ StringHash("W"),			'W',			true },
	{ StringHash("X"),			'X',			true },
	{ StringHash("Y"),			'Y',			true },
	{ StringHash("Z"),			'Z',			true },
	{ StringHash(")"),			'0',			true },
	{ StringHash("!"),			'1',			true },
	{ StringHash("@"),			'2',			true },
	{ StringHash("#"),			'3',			true },
	{ StringHash("$"),			'4',			true },
	{ StringHash("%"),			'5',			true },
	{ StringHash("^"),			'6',			true },
	{ StringHash("&"),			'7',			true },
	{ StringHash("*"),			'8',			true },
	{ StringHash("("),			'9',			true },
	{ StringHash(":"),			VK_OEM_1,		true },
	{ StringHash("?"),			VK_OEM_2,		true },
	{ StringHash("~"),			VK_OEM_3,		true },
	{ StringHash("{"),			VK_OEM_4,		true },
	{ StringHash("|"),			VK_OEM_5,		true },
	{ StringHash("}"),			VK_OEM_6,		true },
	{ StringHash("\""),			VK_OEM_7,		true },
};

static bool _anyButtonDown(roInputDriver* self, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return !impl->_keyDownList.isEmpty();
}

bool _anyButtonUp(roInputDriver* self, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return !impl->_keyUpList.isEmpty();
}

bool _buttonDown(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyDownList.find(buttonName) != NULL;
}

bool _buttonUp(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyUpList.find(buttonName) != NULL;
}

float _mouseAxis(roInputDriver* self, roStringHash axisName)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	if(axisName == stringHash("mouse x"))
		return impl->_mouseAxis.x;
	if(axisName == stringHash("mouse y"))
		return impl->_mouseAxis.y;
	if(axisName == stringHash("mouse z"))
		return impl->_mouseAxis.z;

	return false;
}

float _mouseAxisRaw(roInputDriver* self, roStringHash axisName)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return 0;

	if(axisName == stringHash("mouse x"))
		return impl->_mouseAxisRaw.x;
	if(axisName == stringHash("mouse y"))
		return impl->_mouseAxisRaw.y;
	if(axisName == stringHash("mouse z"))
		return impl->_mouseAxisRaw.z;

	return 0;
}

float _mouseAxisDelta(roInputDriver* self, roStringHash axisName)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return 0;

	if(axisName == stringHash("mouse x"))
		return impl->_mouseAxis.x - impl->_previousMouseAxis.x;
	if(axisName == stringHash("mouse y"))
		return impl->_mouseAxis.y - impl->_previousMouseAxis.y;
	if(axisName == stringHash("mouse z"))
		return impl->_mouseAxis.z - impl->_previousMouseAxis.z;

	return 0;
}

float _mouseAxisDeltaRaw(roInputDriver* self, roStringHash axisName)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return 0;

	if(axisName == stringHash("mouse x"))
		return impl->_mouseAxisRaw.x - impl->_previousMouseAxisRaw.x;
	if(axisName == stringHash("mouse y"))
		return impl->_mouseAxisRaw.y - impl->_previousMouseAxisRaw.y;
	if(axisName == stringHash("mouse z"))
		return impl->_mouseAxisRaw.z - impl->_previousMouseAxisRaw.z;

	return false;
}

bool _mouseButtonDown(roInputDriver* self, int buttonId, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	roUint8 bits = lastFrame ?
		impl->_previousMouseButtonDownBits : impl->_mouseButtonDownBits;

	return (bits & (1 << buttonId)) > 0;
}

bool _mouseButtonUp(roInputDriver* self, int buttonId, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	roUint8 bits = lastFrame ?
		impl->_previousMouseButtonUpBits : impl->_mouseButtonUpBits;

	return (bits & (1 << buttonId)) > 0;
}

void _tick(roInputDriver* self)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return;

	impl->_tick();
}

void roInputDriverImpl::_tick()
{
	_keyDownList.clear();
	_keyUpList.clear();

	_previousMouseButtonDownBits = _mouseButtonDownBits;
	_mouseButtonDownBits = 0;

	_previousMouseButtonUpBits = _mouseButtonUpBits;
	_mouseButtonUpBits = 0;

	_previousMouseAxis = _mouseAxis;
	_previousMouseAxisRaw = _mouseAxisRaw;

	// Perform axis smoothing
	// TODO: Make the smoothing framerate independent
	_mouseAxis = _mouseAxisRaw * 0.5f + _mouseAxis * 0.5f;

	_popWinEvents();
}

void _processEvents(roInputDriver* self, void** data, roSize numData)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return;

	impl->_processEvents(data, numData);
}

void roInputDriverImpl::_processEvents(void** data, roSize numData)
{
	WinEvent e = {
		HWND(data[0]),
		UINT(data[1]),
		WPARAM(data[2]),
		LPARAM(data[3])
	};
	_winEvents.pushBack(e);
}

void roInputDriverImpl::_processButtons(const WinEvent& winEvent)
{
	bool buttonDown = (winEvent.uMsg == WM_KEYDOWN || winEvent.uMsg == WM_SYSKEYDOWN);
	bool buttonUp = (winEvent.uMsg == WM_KEYUP || winEvent.uMsg == WM_SYSKEYUP);
	bool alt = HIWORD(::GetAsyncKeyState(VK_MENU)) != 0;
	bool ctrl = HIWORD(::GetAsyncKeyState(VK_CONTROL)) != 0;
	bool shift = HIWORD(::GetAsyncKeyState(VK_SHIFT)) != 0;

	// Find the mapping for the button event
	for(roSize i=0; i<roCountof(_buttonMapping); ++i) {
		if(winEvent.wParam != _buttonMapping[i].vkCode)
			continue;

		if(!shift && _buttonMapping[i].shift)
			continue;

		roStringHash hash = _buttonMapping[i].hash;
		if(buttonDown) {
			_keyList.pushBack(hash);
			_keyDownList.pushBack(hash);
		}

		if(buttonUp) {
			_keyList.removeAllByKey(hash);
			_keyUpList.pushBack(hash);
		}
	}
}

void roInputDriverImpl::_popWinEvents()
{
	bool buttonCaptured = false;	// TODO: Change this behavior to per button
	bool mouseButtonCaptured = false;
	int mouseButton = -1;

	while(!_winEvents.isEmpty())
	{
		WinEvent e = _winEvents.front();

		switch(e.uMsg)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if(buttonCaptured) return;
			buttonCaptured = true;
			_processButtons(e);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if(buttonCaptured) return;
			buttonCaptured = true;
			_processButtons(e);
			break;

		case WM_MOUSEMOVE:
			_previousMouseAxisRaw.x = _mouseAxisRaw.x;
			_previousMouseAxisRaw.y = _mouseAxisRaw.y;
			{	Vec2 newPos(LOWORD(e.lParam), HIWORD(e.lParam));
				_mouseAxisRaw.x += (newPos.x - _mousePos.x);
				_mouseAxisRaw.y += (newPos.y - _mousePos.y);
				_mousePos = newPos;
			}
			break;

		case WM_MOUSEWHEEL:
			_previousMouseAxisRaw.z = _mouseAxisRaw.z;
			_mouseAxisRaw.z += static_cast<roInt16>(HIWORD(e.wParam)) / 120;
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			if(e.uMsg == WM_LBUTTONDOWN)
				mouseButton = 0;
			if(e.uMsg == WM_MBUTTONDOWN)
				mouseButton = 1;
			if(e.uMsg == WM_RBUTTONDOWN)
				mouseButton = 2;

			if(mouseButtonCaptured) return;
			mouseButtonCaptured = true;
			_mouseButtonDownBits |= (1 << mouseButton);
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			if(e.uMsg == WM_LBUTTONDOWN)
				mouseButton = 0;
			if(e.uMsg == WM_MBUTTONDOWN)
				mouseButton = 1;
			if(e.uMsg == WM_RBUTTONDOWN)
				mouseButton = 2;

			if(mouseButtonCaptured) return;
			mouseButtonCaptured = true;
			_mouseButtonUpBits |= (1 << mouseButton);
			break;

		}

		_winEvents.remove(0);
	}
}

roInputDriver* roNewInputDriver()
{
	AutoPtr<roInputDriverImpl> ret = _allocator.newObj<roInputDriverImpl>();

	return ret.unref();
}

void roInitInputDriver(roInputDriver* self, const char* options)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return;

	roZeroMemory(impl, sizeof(*impl));

	// Setup the function pointers
	impl->anyButtonDown = _anyButtonDown;
	impl->anyButtonUp = _anyButtonUp;
	impl->buttonDown = _buttonDown;
	impl->buttonUp = _buttonUp;

	impl->mouseAxis = _mouseAxis;
	impl->mouseAxisRaw = _mouseAxisRaw;
	impl->mouseAxisDelta = _mouseAxisDelta;
	impl->mouseAxisDeltaRaw = _mouseAxisDeltaRaw;
	impl->mouseButtonDown = _mouseButtonDown;
	impl->mouseButtonUp = _mouseButtonUp;

	impl->tick = _tick;
	impl->processEvents = _processEvents;
}

void roDeleteInputDriver(roInputDriver* self)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return;

	_allocator.deleteObj(impl);
}
