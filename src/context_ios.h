void Rhinoca::processEvent(RhinocaEvent ev)
{
	if(!domWindow)
		return;
	
	const char* touchEvent = (const char*)ev.type;

	if(touchEvent)
	{
		Dom::TouchEvent* e = new Dom::TouchEvent;
		e->type = touchEvent;
		e->screenX = ev.value2;
		e->screenY = ev.value3;
		e->clientX = ev.value2;
		e->clientY = ev.value3;
		e->pageX = ev.value2;
		e->pageY = ev.value3;
		
		e->bind(domWindow->jsContext, NULL);
		
		domWindow->dispatchEvent(e);
	}

	const char* mouseEvent = NULL;
	if(strcmp(touchEvent, "touchstart") == 0) {
		mouseEvent = "mousedown";
	}
	else if(strcmp(touchEvent, "touchmove") == 0) {
		mouseEvent = "mousemove";
	}
	else if(strcmp(touchEvent, "touchend") == 0) {
		mouseEvent = "mouseup";
	}
	else if(strcmp(touchEvent, "touchcancel") == 0) {
		mouseEvent = "mouseup";
	}

	if(mouseEvent)
	{
		Dom::MouseEvent* e = new Dom::MouseEvent;
		e->type = mouseEvent;
		e->screenX = ev.value2;
		e->screenY = ev.value3;
		e->clientX = ev.value2;
		e->clientY = ev.value3;
		e->pageX = ev.value2;
		e->pageY = ev.value3;
		e->button = ev.value1 + 1;

		e->bind(domWindow->jsContext, NULL);
		
		domWindow->dispatchEvent(e);
	}
}
