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
		break;
	case WM_LBUTTONUP:
		mouseEvent = "mouseup";
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
/*
		jsval argv, closure, rval;
		Dom::HTMLDocument* document = domWindow->document;
		if(JS_GetProperty(document->jsContext, *document, keyEvent, &closure) && closure != JSVAL_VOID) {
			Dom::KeyEvent* e = new Dom::KeyEvent;
			e->keyCode = keyCode;
			e->bind(document->jsContext, NULL);
			argv = *e;
			JS_CallFunctionValue(document->jsContext, *document, closure, 1, &argv, &rval);
		}*/
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
