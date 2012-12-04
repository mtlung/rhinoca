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

	roInputDriverImpl();

	void _processEvents(void** data, roSize numData);
	void _processButtons(const WinEvent& e);

	void _popWinEvents();

	void _tick();

	Array<WinEvent> _winEvents;
	Array<ro::StringHash> _keyList, _keyDownList, _keyUpList;

	ZeroInit<roUint8> _mouseButtonDownBits;	// 8 bit can store 8 mouse buttons
	ZeroInit<roUint8> _mouseButtonUpBits;
	ZeroInit<roUint8> _mouseButtonPressBits;
	ZeroInit<roUint8> _previousMouseButtonDownBits;
	ZeroInit<roUint8> _previousMouseButtonUpBits;
	ZeroInit<roUint8> _previousMouseButtonPressBits;

	Vec2 _mousePos;
	Vec3 _mouseAxis, _mouseAxisRaw;
	Vec3 _previousMouseAxis, _previousMouseAxisRaw;

	ZeroInit<bool> _lShiftDown;
	ZeroInit<bool> _rShiftDown;

	ro::String outputText;
	roComPtr<TextService> _textServiceManager;
};

roInputDriverImpl::roInputDriverImpl()
{
	_mousePos = Vec2(0.f);
	_mouseAxis = _mouseAxisRaw = Vec3(0.f);
	_previousMouseAxis = _previousMouseAxisRaw = Vec3(0.f);
}

struct ButtonMapping {
	StringHash hash;
	unsigned vkCode;
};

// http://docs.unity3d.com/Documentation/ScriptReference/KeyCode.html
static ButtonMapping _buttonMapping[] = {
	{ stringHash("Left"),		VK_LEFT		},
	{ stringHash("Right"),		VK_RIGHT	},
	{ stringHash("Up"),			VK_UP		},
	{ stringHash("Down"),		VK_DOWN		},
	{ stringHash("Escape"),		VK_ESCAPE	},
	{ stringHash("Space"),		VK_SPACE	},
	{ stringHash("Return"),		VK_RETURN	},
	{ stringHash("LAlt"),		VK_MENU		},
	{ stringHash("RAlt"),		VK_MENU		},
	{ stringHash("Shift"),		VK_SHIFT	},
	{ stringHash("LShift"),		VK_LSHIFT	},
	{ stringHash("RShift"),		VK_RSHIFT	},
	{ stringHash("LControl"),	VK_CONTROL	},
	{ stringHash("RControl"),	VK_CONTROL	},
	{ stringHash("Numpad0"),	VK_NUMPAD0	},
	{ stringHash("Numpad1"),	VK_NUMPAD1	},
	{ stringHash("Numpad2"),	VK_NUMPAD2	},
	{ stringHash("Numpad3"),	VK_NUMPAD3	},
	{ stringHash("Numpad4"),	VK_NUMPAD4	},
	{ stringHash("Numpad5"),	VK_NUMPAD5	},
	{ stringHash("Numpad6"),	VK_NUMPAD6	},
	{ stringHash("Numpad7"),	VK_NUMPAD7	},
	{ stringHash("Numpad8"),	VK_NUMPAD8	},
	{ stringHash("Numpad9"),	VK_NUMPAD9	},
	{ stringHash("a"),			'A'			},
	{ stringHash("b"),			'B'			},
	{ stringHash("c"),			'C'			},
	{ stringHash("d"),			'D'			},
	{ stringHash("e"),			'E'			},
	{ stringHash("f"),			'F'			},
	{ stringHash("g"),			'G'			},
	{ stringHash("h"),			'H'			},
	{ stringHash("i"),			'I'			},
	{ stringHash("j"),			'J'			},
	{ stringHash("k"),			'K'			},
	{ stringHash("l"),			'L'			},
	{ stringHash("m"),			'M'			},
	{ stringHash("n"),			'N'			},
	{ stringHash("o"),			'O'			},
	{ stringHash("p"),			'P'			},
	{ stringHash("q"),			'Q'			},
	{ stringHash("r"),			'R'			},
	{ stringHash("s"),			'S'			},
	{ stringHash("t"),			'T'			},
	{ stringHash("u"),			'U'			},
	{ stringHash("v"),			'V'			},
	{ stringHash("w"),			'W'			},
	{ stringHash("x"),			'X'			},
	{ stringHash("y"),			'Y'			},
	{ stringHash("z"),			'Z'			},
	{ stringHash("0"),			'0'			},
	{ stringHash("1"),			'1'			},
	{ stringHash("2"),			'2'			},
	{ stringHash("3"),			'3'			},
	{ stringHash("4"),			'4'			},
	{ stringHash("5"),			'5'			},
	{ stringHash("6"),			'6'			},
	{ stringHash("7"),			'7'			},
	{ stringHash("8"),			'8'			},
	{ stringHash("9"),			'9'			},
	{ stringHash(";"),			VK_OEM_1	},
	{ stringHash("/"),			VK_OEM_2	},
	{ stringHash("`"),			VK_OEM_3	},
	{ stringHash("["),			VK_OEM_4	},
	{ stringHash("\\"),			VK_OEM_5	},
	{ stringHash("]"),			VK_OEM_6	},
	{ stringHash("'"),			VK_OEM_7	},
	{ stringHash("+"),			VK_OEM_PLUS	},
	{ stringHash("-"),			VK_OEM_MINUS},
	{ stringHash(","),			VK_OEM_COMMA},
	{ stringHash("."),			VK_OEM_PERIOD},
	{ stringHash("Back"),		VK_BACK		},
	{ stringHash("Tab"),		VK_TAB		},
	{ stringHash("PageUp"),		VK_PRIOR	},
	{ stringHash("PageDown"),	VK_NEXT		},
	{ stringHash("End"),		VK_END		},
	{ stringHash("Home"),		VK_HOME		},
	{ stringHash("Insert"),		VK_INSERT	},
	{ stringHash("Delete"),		VK_DELETE	},
	{ stringHash("Add"),		VK_ADD		},
	{ stringHash("Subtract"),	VK_SUBTRACT	},
	{ stringHash("Multiply"),	VK_MULTIPLY	},
	{ stringHash("Divide"),		VK_DIVIDE	},
	{ stringHash("Pause"),		VK_PAUSE	},
	{ stringHash("F1"),			VK_F1		},
	{ stringHash("F2"),			VK_F2		},
	{ stringHash("F3"),			VK_F3		},
	{ stringHash("F4"),			VK_F4		},
	{ stringHash("F5"),			VK_F5		},
	{ stringHash("F6"),			VK_F6		},
	{ stringHash("F7"),			VK_F7		},
	{ stringHash("F8"),			VK_F8		},
	{ stringHash("F9"),			VK_F9		},
	{ stringHash("F10"),		VK_F10		},
	{ stringHash("F11"),		VK_F11		},
	{ stringHash("F12"),		VK_F12		},
	{ stringHash("F13"),		VK_F13		},
	{ stringHash("F14"),		VK_F14		},
	{ stringHash("F15"),		VK_F15		},
	{ stringHash("LSystem"),	VK_LWIN		},
	{ stringHash("RSystem"),	VK_RWIN		},
	{ stringHash("Menu"),		VK_APPS		},
	{ stringHash("A"),			'A'			},
	{ stringHash("B"),			'B'			},
	{ stringHash("C"),			'C'			},
	{ stringHash("D"),			'D'			},
	{ stringHash("E"),			'E'			},
	{ stringHash("F"),			'F'			},
	{ stringHash("G"),			'G'			},
	{ stringHash("H"),			'H'			},
	{ stringHash("I"),			'I'			},
	{ stringHash("J"),			'J'			},
	{ stringHash("K"),			'K'			},
	{ stringHash("L"),			'L'			},
	{ stringHash("M"),			'M'			},
	{ stringHash("N"),			'N'			},
	{ stringHash("O"),			'O'			},
	{ stringHash("P"),			'P'			},
	{ stringHash("Q"),			'Q'			},
	{ stringHash("R"),			'R'			},
	{ stringHash("S"),			'S'			},
	{ stringHash("T"),			'T'			},
	{ stringHash("U"),			'U'			},
	{ stringHash("V"),			'V'			},
	{ stringHash("W"),			'W'			},
	{ stringHash("X"),			'X'			},
	{ stringHash("Y"),			'Y'			},
	{ stringHash("Z"),			'Z'			},
	{ stringHash(")"),			'0'			},
	{ stringHash("!"),			'1'			},
	{ stringHash("@"),			'2'			},
	{ stringHash("#"),			'3'			},
	{ stringHash("$"),			'4'			},
	{ stringHash("%"),			'5'			},
	{ stringHash("^"),			'6'			},
	{ stringHash("&"),			'7'			},
	{ stringHash("*"),			'8'			},
	{ stringHash("("),			'9'			},
	{ stringHash(":"),			VK_OEM_1	},
	{ stringHash("?"),			VK_OEM_2	},
	{ stringHash("~"),			VK_OEM_3	},
	{ stringHash("{"),			VK_OEM_4	},
	{ stringHash("|"),			VK_OEM_5	},
	{ stringHash("}"),			VK_OEM_6	},
	{ stringHash("\""),			VK_OEM_7	},
};

static bool _buttonUp(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyUpList.find(buttonName) != NULL;
}

static bool _buttonDown(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyDownList.find(buttonName) != NULL;
}

static bool _buttonPressed(roInputDriver* self, roStringHash buttonName, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return impl->_keyList.find(buttonName) != NULL;
}

static bool _anyButtonUp(roInputDriver* self, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return !impl->_keyUpList.isEmpty();
}

static bool _anyButtonDown(roInputDriver* self, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	return !impl->_keyDownList.isEmpty();
}

static float _mouseAxis(roInputDriver* self, roStringHash axisName)
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

static float _mouseAxisRaw(roInputDriver* self, roStringHash axisName)
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

static float _mouseAxisDelta(roInputDriver* self, roStringHash axisName)
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

static float _mouseAxisDeltaRaw(roInputDriver* self, roStringHash axisName)
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

static bool _mouseButtonUp(roInputDriver* self, int buttonId, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	roUint8 bits = lastFrame ?
		impl->_previousMouseButtonUpBits : impl->_mouseButtonUpBits;

	return (bits & (1 << buttonId)) > 0;
}

static bool _mouseButtonDown(roInputDriver* self, int buttonId, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	roUint8 bits = lastFrame ?
		impl->_previousMouseButtonDownBits : impl->_mouseButtonDownBits;

	return (bits & (1 << buttonId)) > 0;
}

static bool _mouseButtonPressed(roInputDriver* self, int buttonId, bool lastFrame)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return false;

	roUint8 bits = lastFrame ?
		impl->_previousMouseButtonPressBits : impl->_mouseButtonPressBits;

	return (bits & (1 << buttonId)) > 0;
}

static const roUtf8* _inputText(roInputDriver* self)
{
	roInputDriverImpl* impl = static_cast<roInputDriverImpl*>(self);
	if(!impl) return "";

	return impl->outputText.c_str();
}

static void _tick(roInputDriver* self)
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

	_previousMouseButtonPressBits = _mouseButtonPressBits;

	_previousMouseAxis = _mouseAxis;
	_previousMouseAxisRaw = _mouseAxisRaw;

	// Perform axis smoothing
	// TODO: Make the smoothing framerate independent
	_mouseAxis = _mouseAxisRaw * 0.5f + _mouseAxis * 0.5f;

	_textServiceManager->popText(outputText);

	_popWinEvents();
}

static void _processEvents(roInputDriver* self, void** data, roSize numData)
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
			_mouseButtonPressBits |= (1 << mouseButton);
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
			else {
				_mouseButtonUpBits |= (1 << mouseButton);
				_mouseButtonPressBits &= ~(1 << mouseButton);
			}
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

	// Setup the function pointers
	impl->buttonUp = _buttonUp;
	impl->buttonDown = _buttonDown;
	impl->buttonPressed = _buttonPressed;
	impl->anyButtonUp = _anyButtonUp;
	impl->anyButtonDown = _anyButtonDown;

	impl->mouseAxis = _mouseAxis;
	impl->mouseAxisRaw = _mouseAxisRaw;
	impl->mouseAxisDelta = _mouseAxisDelta;
	impl->mouseAxisDeltaRaw = _mouseAxisDeltaRaw;
	impl->mouseButtonUp = _mouseButtonUp;
	impl->mouseButtonDown = _mouseButtonDown;
	impl->mouseButtonPressed = _mouseButtonPressed;

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
