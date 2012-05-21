void Rhinoca::processEvent(RhinocaEvent ev)
{
	if(!domWindow)
		return;
	
	const char* touchEvent = (const char*)ev.type;
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
	
	{	Dom::TouchData& data = domWindow->allocateTouchData(ev.value1);
		data.screenX = ev.value2;
		data.screenY = ev.value3;
		data.clientX = ev.value2;
		data.clientY = ev.value3;
		data.pageX = ev.value2;
		data.pageY = ev.value3;

		Dom::TouchEvent* e = new Dom::TouchEvent;
		e->type = touchEvent;
		e->touches = domWindow->touches;
		e->changedTouches.pushBack(data);

		e->bind(domWindow->jsContext, NULL);

		domWindow->dispatchEvent(e);
	}

	// Generate mouse event for single touch event
	// http://vetruvet.blogspot.com/2010/12/converting-single-touch-events-to-mouse.html
	if(domWindow->touches.size() == 1)
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
