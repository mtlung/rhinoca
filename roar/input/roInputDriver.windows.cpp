#include "pch.h"
#include "roInputDriver.h"
#include "../base/roArray.h"
#include "../base/roComPtr.h"
#include "../base/roString.h"
#include "../base/roStringHash.h"
#include "../math/roVector.h"
#include "../platform/roPlatformHeaders.h"

#include "roInputDriver.ime.windows.inl"

using namespace ro;

// Some notes on win input message for game:
// http://dominikgrabiec.com/26-game-engine-input/
// http://www.yaldex.com/games-programming/0672323699_ch03lev1sec4.html

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

	bool _lShiftDown;
	bool _rShiftDown;

	ro::String outputText;
	roComPtr<TextService> _textServiceManager;
};

struct ButtonMapping {
	StringHash hash;
	unsigned vkCode;
	bool shift;
};

// http://docs.unity3d.com/Documentation/ScriptReference/KeyCode.html
static ButtonMapping _buttonMapping[] = {
	{ stringHash("Left"),		VK_LEFT,		false },
	{ stringHash("Right"),		VK_RIGHT,		false },
	{ stringHash("Up"),			VK_UP,			false },
	{ stringHash("Down"),		VK_DOWN,		false },
	{ stringHash("Escape"),		VK_ESCAPE,		false },
	{ stringHash("Space"),		VK_SPACE,		false },
	{ stringHash("Return"),		VK_RETURN,		false },
	{ stringHash("LAlt"),		VK_MENU,		false },
	{ stringHash("RAlt"),		VK_MENU,		false },
	{ stringHash("Shift"),		VK_SHIFT,		false },
	{ stringHash("LShift"),		VK_LSHIFT,		false },
	{ stringHash("RShift"),		VK_RSHIFT,		false },
	{ stringHash("LControl"),	VK_CONTROL,		false },
	{ stringHash("RControl"),	VK_CONTROL,		false },
	{ stringHash("Numpad0"),	VK_NUMPAD0,		false },
	{ stringHash("Numpad1"),	VK_NUMPAD1,		false },
	{ stringHash("Numpad2"),	VK_NUMPAD2,		false },
	{ stringHash("Numpad3"),	VK_NUMPAD3,		false },
	{ stringHash("Numpad4"),	VK_NUMPAD4,		false },
	{ stringHash("Numpad5"),	VK_NUMPAD5,		false },
	{ stringHash("Numpad6"),	VK_NUMPAD6,		false },
	{ stringHash("Numpad7"),	VK_NUMPAD7,		false },
	{ stringHash("Numpad8"),	VK_NUMPAD8,		false },
	{ stringHash("Numpad9"),	VK_NUMPAD9,		false },
	{ stringHash("a"),			'A',			false },
	{ stringHash("b"),			'B',			false },
	{ stringHash("c"),			'C',			false },
	{ stringHash("d"),			'D',			false },
	{ stringHash("e"),			'E',			false },
	{ stringHash("f"),			'F',			false },
	{ stringHash("g"),			'G',			false },
	{ stringHash("h"),			'H',			false },
	{ stringHash("i"),			'I',			false },
	{ stringHash("j"),			'J',			false },
	{ stringHash("k"),			'K',			false },
	{ stringHash("l"),			'L',			false },
	{ stringHash("m"),			'M',			false },
	{ stringHash("n"),			'N',			false },
	{ stringHash("o"),			'O',			false },
	{ stringHash("p"),			'P',			false },
	{ stringHash("q"),			'Q',			false },
	{ stringHash("r"),			'R',			false },
	{ stringHash("s"),			'S',			false },
	{ stringHash("t"),			'T',			false },
	{ stringHash("u"),			'U',			false },
	{ stringHash("v"),			'V',			false },
	{ stringHash("w"),			'W',			false },
	{ stringHash("x"),			'X',			false },
	{ stringHash("y"),			'Y',			false },
	{ stringHash("z"),			'Z',			false },
	{ stringHash("0"),			'0',			false },
	{ stringHash("1"),			'1',			false },
	{ stringHash("2"),			'2',			false },
	{ stringHash("3"),			'3',			false },
	{ stringHash("4"),			'4',			false },
	{ stringHash("5"),			'5',			false },
	{ stringHash("6"),			'6',			false },
	{ stringHash("7"),			'7',			false },
	{ stringHash("8"),			'8',			false },
	{ stringHash("9"),			'9',			false },
	{ stringHash(";"),			VK_OEM_1,		false },
	{ stringHash("/"),			VK_OEM_2,		false },
	{ stringHash("`"),			VK_OEM_3,		false },
	{ stringHash("["),			VK_OEM_4,		false },
	{ stringHash("\\"),			VK_OEM_5,		false },
	{ stringHash("]"),			VK_OEM_6,		false },
	{ stringHash("'"),			VK_OEM_7,		false },
	{ stringHash("+"),			VK_OEM_PLUS,	false },
	{ stringHash("-"),			VK_OEM_MINUS,	false },
	{ stringHash(","),			VK_OEM_COMMA,	false },
	{ stringHash("."),			VK_OEM_PERIOD,	false },
	{ stringHash("Back"),		VK_BACK,		false },
	{ stringHash("Tab"),		VK_TAB,			false },
	{ stringHash("PageUp"),		VK_PRIOR,		false },
	{ stringHash("PageDown"),	VK_NEXT,		false },
	{ stringHash("End"),		VK_END,			false },
	{ stringHash("Home"),		VK_HOME,		false },
	{ stringHash("Insert"),		VK_INSERT,		false },
	{ stringHash("Delete"),		VK_DELETE,		false },
	{ stringHash("Add"),		VK_ADD,			false },
	{ stringHash("Subtract"),	VK_SUBTRACT,	false },
	{ stringHash("Multiply"),	VK_MULTIPLY,	false },
	{ stringHash("Divide"),		VK_DIVIDE,		false },
	{ stringHash("Pause"),		VK_PAUSE,		false },
	{ stringHash("F1"),			VK_F1,			false },
	{ stringHash("F2"),			VK_F2,			false },
	{ stringHash("F3"),			VK_F3,			false },
	{ stringHash("F4"),			VK_F4,			false },
	{ stringHash("F5"),			VK_F5,			false },
	{ stringHash("F6"),			VK_F6,			false },
	{ stringHash("F7"),			VK_F7,			false },
	{ stringHash("F8"),			VK_F8,			false },
	{ stringHash("F9"),			VK_F9,			false },
	{ stringHash("F10"),		VK_F10,			false },
	{ stringHash("F11"),		VK_F11,			false },
	{ stringHash("F12"),		VK_F12,			false },
	{ stringHash("F13"),		VK_F13,			false },
	{ stringHash("F14"),		VK_F14,			false },
	{ stringHash("F15"),		VK_F15,			false },
	{ stringHash("LSystem"),	VK_LWIN,		false },
	{ stringHash("RSystem"),	VK_RWIN,		false },
	{ stringHash("Menu"),		VK_APPS,		false },
	{ stringHash("A"),			'A',			true },
	{ stringHash("B"),			'B',			true },
	{ stringHash("C"),			'C',			true },
	{ stringHash("D"),			'D',			true },
	{ stringHash("E"),			'E',			true },
	{ stringHash("F"),			'F',			true },
	{ stringHash("G"),			'G',			true },
	{ stringHash("H"),			'H',			true },
	{ stringHash("I"),			'I',			true },
	{ stringHash("J"),			'J',			true },
	{ stringHash("K"),			'K',			true },
	{ stringHash("L"),			'L',			true },
	{ stringHash("M"),			'M',			true },
	{ stringHash("N"),			'N',			true },
	{ stringHash("O"),			'O',			true },
	{ stringHash("P"),			'P',			true },
	{ stringHash("Q"),			'Q',			true },
	{ stringHash("R"),			'R',			true },
	{ stringHash("S"),			'S',			true },
	{ stringHash("T"),			'T',			true },
	{ stringHash("U"),			'U',			true },
	{ stringHash("V"),			'V',			true },
	{ stringHash("W"),			'W',			true },
	{ stringHash("X"),			'X',			true },
	{ stringHash("Y"),			'Y',			true },
	{ stringHash("Z"),			'Z',			true },
	{ stringHash(")"),			'0',			true },
	{ stringHash("!"),			'1',			true },
	{ stringHash("@"),			'2',			true },
	{ stringHash("#"),			'3',			true },
	{ stringHash("$"),			'4',			true },
	{ stringHash("%"),			'5',			true },
	{ stringHash("^"),			'6',			true },
	{ stringHash("&"),			'7',			true },
	{ stringHash("*"),			'8',			true },
	{ stringHash("("),			'9',			true },
	{ stringHash(":"),			VK_OEM_1,		true },
	{ stringHash("?"),			VK_OEM_2,		true },
	{ stringHash("~"),			VK_OEM_3,		true },
	{ stringHash("{"),			VK_OEM_4,		true },
	{ stringHash("|"),			VK_OEM_5,		true },
	{ stringHash("}"),			VK_OEM_6,		true },
	{ stringHash("\""),			VK_OEM_7,		true },
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

bool _button(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyList.find(buttonName) != NULL;
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

const roUtf8* _inputText(roInputDriver* self)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return "";

	return impl->outputText.c_str();
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

	_textServiceManager->popText(outputText);

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
	WPARAM wParam = winEvent.wParam;

HandleLRShift:
	// Find the mapping for the button event
	for(roSize i=0; i<roCountof(_buttonMapping); ++i) {
		if(wParam != _buttonMapping[i].vkCode)
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

		break;
	}

	// Special handling to determine left/right shift key
	if(wParam == VK_SHIFT) {
		if(!_lShiftDown && (GetKeyState(VK_LSHIFT) & 0x8000)) {
			_lShiftDown = true;
			wParam = VK_LSHIFT;
		}
		if(_lShiftDown && !(GetKeyState(VK_LSHIFT) & 0x8000)) {
			_lShiftDown = false;
			wParam = VK_LSHIFT;
		}

		if(!_rShiftDown && (GetKeyState(VK_RSHIFT) & 0x8000)) {
			_rShiftDown = true;
			wParam = VK_RSHIFT;
		}
		if(_rShiftDown && !(GetKeyState(VK_RSHIFT) & 0x8000)) {
			_rShiftDown = false;
			wParam = VK_RSHIFT;
		}

		if(wParam != VK_SHIFT)
			goto HandleLRShift;
	}
}

// Mouse position can be negative (when it move out side the window)
// therefore we can't simple use LOWORD and HIWORD
// See http://msdn.microsoft.com/en-us/library/windows/desktop/ms632654%28v=vs.85%29.aspx
#ifndef GET_X_LPARAM
#	define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#	define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

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
			{	Vec2 newPos(float(GET_X_LPARAM(e.lParam)), float(GET_Y_LPARAM(e.lParam)));
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
			SetCapture(e.hWnd);
			mouseButtonCaptured = true;
			_mouseButtonDownBits |= (1 << mouseButton);
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_KILLFOCUS:
			if(e.uMsg == WM_LBUTTONUP)
				mouseButton = 0;
			if(e.uMsg == WM_MBUTTONUP)
				mouseButton = 1;
			if(e.uMsg == WM_RBUTTONUP)
				mouseButton = 2;

			if(mouseButtonCaptured) return;
			ReleaseCapture();
			mouseButtonCaptured = true;

			// All button up when losing focus
			if(e.uMsg == WM_KILLFOCUS)
				_mouseButtonUpBits = 0xFF;
			else
				_mouseButtonUpBits |= (1 << mouseButton);
			break;

		case WM_CHAR:
		{
			roUint16 wchar[2] = { num_cast<roUint16>(e.wParam), L'\0' };
			static const roUint16 include[] = {
				0x9,	// Horizontal tab
				0xA,	// New line
				0xD,	// Line feed
			};

			if(wchar[0] >= 0x20 || roArrayFind(include, roCountof(include), wchar[0])) {
				if(wchar[0] == 0xD)	// Change new feed char to new line char
					wchar[0] = 0xA;
				ro::String tmp;
				tmp.fromUtf16(wchar);
				outputText += tmp;
			}
		}	break;
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
	impl->button = _button;

	impl->mouseAxis = _mouseAxis;
	impl->mouseAxisRaw = _mouseAxisRaw;
	impl->mouseAxisDelta = _mouseAxisDelta;
	impl->mouseAxisDeltaRaw = _mouseAxisDeltaRaw;
	impl->mouseButtonDown = _mouseButtonDown;
	impl->mouseButtonUp = _mouseButtonUp;

	impl->_lShiftDown = false;
	impl->_rShiftDown = false;

	impl->inputText = _inputText;

	impl->tick = _tick;
	impl->processEvents = _processEvents;

	impl->_textServiceManager = new TextService();
	impl->_textServiceManager->init();
}

void roDeleteInputDriver(roInputDriver* self)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return;

	impl->_textServiceManager->close();

	_allocator.deleteObj(impl);
}
