int convertKeyCode(WPARAM virtualKey, LPARAM flags)
{
	switch(virtualKey)
	{
		case VK_SPACE :			return 32;
		case VK_LEFT :			return 37;
		case VK_RIGHT :			return 39;
		case VK_UP :			return 38;
		case VK_DOWN :			return 0;
	}

	return 0;
}

void Rhinoca::processEvent(RhinocaEvent ev)
{
	if(!domWindow)
		return;

	WPARAM wParam = ev.value1;
	LPARAM lParam = ev.value2;

	const char* onkeydown = "onkeydown";
	const char* onkeyup = "onkeyup";
	const char* keyEvent = NULL;
	int keyCode = 0;

	const char* mouseEvent = NULL;

	switch(ev.type)
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
		mouseEvent = "onmousedown";
		break;
	case WM_LBUTTONUP:
		mouseEvent = "onmouseup";
		break;
	case WM_MOUSEMOVE:
		mouseEvent = "onmousemove";
		break;
	}

	if(keyEvent)
	{
		jsval argv, closure, rval;
		Dom::HTMLDocument* document = domWindow->document;
		if(JS_GetProperty(document->jsContext, document->jsObject, keyEvent, &closure) && closure != JSVAL_VOID) {
			Dom::KeyEvent* e = new Dom::KeyEvent;
			e->keyCode = keyCode;
			e->bind(document->jsContext, NULL);
			argv = OBJECT_TO_JSVAL(e->jsObject);
			JS_CallFunctionValue(document->jsContext, document->jsObject, closure, 1, &argv, &rval);
		}
	}

	if(mouseEvent) for(Dom::NodeIterator itr(domWindow->document->rootNode()); !itr.ended(); itr.next())
	{
		jsval argv, closure, rval;
		if(JS_GetProperty(itr->jsContext, itr->jsObject, mouseEvent, &closure) && closure != JSVAL_VOID) {
			Dom::MouseEvent* e = new Dom::MouseEvent;
			e->clientX = LOWORD(lParam);
			e->clientY = HIWORD(lParam);
			e->bind(itr->jsContext, NULL);
			argv = OBJECT_TO_JSVAL(e->jsObject);
			JS_CallFunctionValue(itr->jsContext, itr->jsObject, closure, 1, &argv, &rval);
		}
	}
}
