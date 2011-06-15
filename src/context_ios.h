void Rhinoca::processEvent(RhinocaEvent ev)
{
	if(!domWindow)
		return;
	
	const char* name = (const char*)ev.type;
	const char* mouseEvent = NULL;
	if(strcmp(name, "touchstart") == 0) {
		mouseEvent = "mousedown";
	}
	else if(strcmp(name, "touchmove") == 0) {
		mouseEvent = "mousemove";
	}
	else if(strcmp(name, "touchend") == 0) {
		mouseEvent = "mouseup";
	}
	else if(strcmp(name, "touchcancel") == 0) {
		mouseEvent = "mouseup";
	}
	
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
