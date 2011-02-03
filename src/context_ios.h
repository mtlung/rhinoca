void Rhinoca::processEvent(RhinocaEvent ev)
{
	const char* mouseEvent = NULL;
	
	if(mouseEvent) for(Dom::NodeIterator itr(domWindow->document->rootNode()); !itr.ended(); itr.next())
	{
		jsval argv, closure, rval;
		if(JS_GetProperty(itr->jsContext, itr->jsObject, mouseEvent, &closure) && closure != JSVAL_VOID) {
			Dom::MouseEvent* e = new Dom::MouseEvent;
			e->clientX = ev.value1;
			e->clientY = ev.value2;
			e->bind(itr->jsContext, NULL);
			argv = OBJECT_TO_JSVAL(e->jsObject);
			JS_CallFunctionValue(itr->jsContext, itr->jsObject, closure, 1, &argv, &rval);
		}
	}
}
