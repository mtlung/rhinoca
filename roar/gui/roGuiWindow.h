GuiWindowState::GuiWindowState()
{
	userData = NULL;
	windowFunction = NULL;
	_useCount = 0;
}

static void _drawWindows()
{
	// Determine which window haven't been use for this frame
	// Use a temp list to prevent the window callback making changes to _states.windowList
	TinyArray<GuiWindowState*, 32> tmpWindows;
	for(roSize i=0; i<_states.windowList.size(); ++i) {
		GuiWindowState* state = _states.windowList[i];

		if(!state || state->_useCount == 1) {
			_states.windowList.remove(i);
			--i;
			continue;
		}
		state->_useCount--;

		tmpWindows.pushBack(state);
	}

	// Determine which window is clicked and make it active
	if(_states.mouseDown()) {
		TinyArray<GuiWindowState*, 32> tmpWindows2 = tmpWindows;
		tmpWindows2.removeByKey(&_states.rootWindow);
		tmpWindows2.insert(0, &_states.rootWindow);

		for(roSize i = tmpWindows2.size(); i--;) {
			GuiWindowState& state = *tmpWindows2[i];
			_states.currentProcessingWindow = _states.windowList.back();	// Force guiIsInActiveWindow() to pass
			if(_isClickedDown(state.deducedRect)) {
				guiFocusWindow(state);
				break;
			}
		}
	}

	for(roSize i = tmpWindows.size(); i--;) {
		GuiWindowState& state = *tmpWindows[i];
		state.mouseClickPos.x = _states.mousex() - state.clientRect.x;
		state.mouseClickPos.y = _states.mousey() - state.clientRect.y;
	}

	for(roSize i=0; i<tmpWindows.size(); ++i) {
		GuiWindowState& state = *tmpWindows[i];
		_states.currentProcessingWindow = &state;

		// Nothing to draw for the root window, just skip it
		if(&state == &_states.rootWindow)
			continue;

		// Handle mouse drag on the title bar
		state.titleBar.rect = state.deducedRect;
		state.titleBar.rect.h = 20;
		guiButtonLogic(state.titleBar);
		if(state.titleBar.isActive) {
			state.rect.x += _states.mousedx();
			state.rect.y += _states.mousedy();
		}

		// Draw the title
		GuiStyle titleStyle = guiSkin.windowTitle;
		if(i == tmpWindows.size() - 1) {
			titleStyle.normal = titleStyle.active;
			titleStyle.hover = titleStyle.active;
		}
		else {
			titleStyle.hover = titleStyle.normal;
			titleStyle.active = titleStyle.normal;
		}
		guiButtonDraw(state.titleBar, state.title.c_str(), &titleStyle);

		// Update deducedRect
		GuiStyle& bgStyle = guiSkin.window;
		state.deducedRect = state.rect;
		state.deducedRect.x -= bgStyle.border;
		state.deducedRect.w += bgStyle.border * 2;
		state.deducedRect.y -= bgStyle.border;
		state.deducedRect.h += bgStyle.border * 2;

		if(!_states.containerStack.isEmpty())
			_mergeExtend(_states.containerStack.back()->virtualRect, state.deducedRect);

		// Update the client area
		state.clientRect = state.rect;
		state.virtualRect = state.clientRect;

		// Draw background
		guiDrawBox(state, NULL, bgStyle, true, true);

		state.deducedRect.y -= state.titleBar.deducedRect.h;
		state.deducedRect.h += state.titleBar.deducedRect.h;

		// Process the window's content
		if(state.windowFunction) {
			_states.currentProcessingWindow = &state;
			guiBeginContainer(state);

			(*state.windowFunction)(state);

			guiEndContainer();
			_states.currentProcessingWindow = NULL;
		}
	}
}

void guiWindow(GuiWindowState& state, const GuiStyle* style)
{
	if(state._useCount == 0) {
		_states.windowList.pushBack(&state);
		state._useCount++;
	}
	state._useCount++;
}

void guiFocusWindow(GuiWindowState& state)
{
	if(_states.windowList.removeByKey(&state))
		_states.windowList.pushBack(&state);
}

bool guiIsInActiveWindow()
{
	if(_states.windowList.isEmpty())
		return true;
	return _states.currentProcessingWindow == _states.windowList.back();
}

bool guiIsObstructedByOtherWindow(float x, float y)
{
	// Convert to global coordinate
	const Vec2& offset = _states.offsetStack.back();
	x -= offset.x;
	y -= offset.y;

	for(roSize i=_states.windowList.size(); i--;) {
		GuiWindowState& state = *_states.windowList[i];

		if(&state == _states.currentProcessingWindow)
			break;

		if(state.deducedRect.isPointInRect(x, y))
			return true;
	}

	return false;
}
