int convertKeyCode(WPARAM key, LPARAM flags)
{
	switch(key)
	{
		case VK_SPACE :		return 32;
		case VK_LEFT :		return 37;
		case VK_RIGHT :		return 39;
		case VK_UP :		return 38;
		case VK_DOWN :		return 0;
	}

	return key;
}

void Rhinoca::processEvent(RhinocaEvent ev)
{
	if(!domWindow)
		return;

	WPARAM wParam = ev.value1;
	LPARAM lParam = ev.value2;

	const char* onkeydown = "keydown";
	const char* onkeyup = "keyup";
	const char* keyEvent = NULL;
	int keyCode = 0;

	const char* mouseEvent = NULL;
	const char* touchEvent = NULL;

	switch((UINT)ev.type)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if(/*myKeyRepeatEnabled || */((lParam & (1 << 30)) == 0)) {
			keyCode = convertKeyCode(wParam, lParam);
			keyEvent = onkeydown;
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		keyCode = convertKeyCode(wParam, lParam);
		keyEvent = onkeyup;
		break;

	case WM_LBUTTONDOWN:
		mouseEvent = "mousedown";
		touchEvent = "touchstart";
		break;
	case WM_LBUTTONUP:
		mouseEvent = "mouseup";
		touchEvent = "touchend";
		break;
	case WM_MOUSEMOVE:
		mouseEvent = "mousemove";
		break;
	}

	if(keyEvent)
	{
		Dom::KeyEvent* e = new Dom::KeyEvent;
		e->type = keyEvent;
		e->keyCode = keyCode;

		e->bind(domWindow->jsContext, NULL);
		e->target = domWindow;

		domWindow->dispatchEvent(e);
	}

	// The standard require dispatching the touch event first, and then mouse event
	// See http://dvcs.w3.org/hg/webevents/raw-file/tip/touchevents.html#mouse-events
	if(touchEvent)
	{
		// We use identifier '0' to represent the mouse
		Dom::TouchData& data = domWindow->allocateTouchData(0);
		data.screenX = LOWORD(lParam);
		data.screenY = HIWORD(lParam);
		data.clientX = LOWORD(lParam);
		data.clientY = HIWORD(lParam);
		data.pageX = LOWORD(lParam);
		data.pageY = HIWORD(lParam);

		Dom::TouchEvent* e = new Dom::TouchEvent;
		e->type = touchEvent;
		e->touches = domWindow->touches;
		e->changedTouches.pushBack(data);

		e->bind(domWindow->jsContext, NULL);

		domWindow->dispatchEvent(e);
	}

	if(mouseEvent)
	{
		Dom::MouseEvent* e = new Dom::MouseEvent;
		e->type = mouseEvent;
		e->screenX = LOWORD(lParam);
		e->screenY = HIWORD(lParam);
		e->clientX = LOWORD(lParam);
		e->clientY = HIWORD(lParam);
		e->pageX = LOWORD(lParam);
		e->pageY = HIWORD(lParam);
		e->shiftKey = (wParam & MK_SHIFT) > 0;
		e->altKey = (GetKeyState(VK_MENU) & 0x8000) > 0;
		e->ctrlKey = (wParam & MK_CONTROL) > 0;
		e->button =
			1 * ((wParam & MK_LBUTTON) > 0) +
			2 * ((wParam & MK_MBUTTON) > 0) +
			3 * ((wParam & MK_RBUTTON) > 0);

		e->bind(domWindow->jsContext, NULL);

		domWindow->dispatchEvent(e);
	}
}
