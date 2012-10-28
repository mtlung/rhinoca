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
		roAssert(_states.windowList[i]);
		GuiWindowState& state = *_states.windowList[i];

		if(state._useCount == 1) {
			_states.windowList.remove(i);
			--i;
			continue;
		}
		state._useCount--;

		tmpWindows.pushBack(&state);
	}

	// Determine which window is clicked and make it active
	for(roSize i = tmpWindows.size(); i--;) {
		GuiWindowState& state = *tmpWindows[i];
		_states.currentProcessingWindow = _states.windowList.back();	// Force guiIsInActiveWindow() to pass
		if(_isClickedDown(state.deducedRect)) {
			guiFocusWindow(state);
			break;
		}
	}

	if(_states.mouseDown()) for(roSize i = tmpWindows.size(); i--;) {
		GuiWindowState& state = *tmpWindows[i];
		state.mouseClickPos.x = _states.mousex() - state.clientRect.x;
		state.mouseClickPos.y = _states.mousey() - state.clientRect.y;
	}

	for(roSize i=0; i<tmpWindows.size(); ++i) {
		GuiWindowState& state = *tmpWindows[i];
		_states.currentProcessingWindow = _states.windowList.back();	// Force guiIsInActiveWindow() to pass

		// Handle mouse drag on the title bar
		state.titleBar.rect = state.deducedRect;
		state.titleBar.rect.h = 20;
		guiButtonLogic(state.titleBar);
		if(_states.hotObject == &state.titleBar) {
			state.rect.x += _states.mousedx();
			state.rect.y += _states.mousedy();
		}
		guiButtonDraw(state.titleBar, state.title.c_str(), &guiSkin.windowTitle);

		// Update deducedRect
		GuiStyle& style = guiSkin.window;
		state.deducedRect = state.rect;
		state.deducedRect.x -= style.border;
		state.deducedRect.w += style.border * 2;
		state.deducedRect.y -= style.border;
		state.deducedRect.h += style.border * 2;

		// Update the client area
		state.clientRect = state.rect;
		state.virtualRect = state.clientRect;

		// Draw background
		guiDrawBox(state, NULL, style, true, true);

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
