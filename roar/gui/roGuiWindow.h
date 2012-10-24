GuiWindowState::GuiWindowState()
{
	userData = NULL;
	windowFunction = NULL;
	_useCount = 0;
}

static void _drawWindows()
{
	Canvas& c = *_states.canvas;

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
		if(_isClickedDown(state.deducedRect) || _isClickedDown(state.titleBar.deducedRect)) {
			guiFocusWindow(state);
			break;
		}
	}

	for(roSize i=0; i<tmpWindows.size(); ++i) {
		GuiWindowState& state = *tmpWindows[i];

		GuiStyle& style = guiSkin.window;
		state.deducedRect = state.rect;

		// Update the client area
		Rectf& clientRect = state._clientRect;
		clientRect = state.deducedRect;
		clientRect.x += style.border;
		clientRect.w -= style.border * 2;
		clientRect.y += style.border;
		clientRect.h -= style.border * 2;

		// Draw background
		guiDrawBox(state, NULL, style, true, true);

		// Draw title bar
		state.titleBar.rect = state.deducedRect;
		state.titleBar.rect.h = 10;	// This is the minimum bar height

		Sizef textExtend(c.lineSpacing(), c.lineSpacing());
		GuiStyle dummyStyle;
		_setContentExtend(state.titleBar, dummyStyle, textExtend);
		state.titleBar.deducedRect.y -= state.titleBar.deducedRect.h;
		guiButtonDraw(state.titleBar, state.title.c_str(), &guiSkin.windowTitle);

		// Process the window's content
		if(state.windowFunction) {
			_states.currentProcessingWindow = &state;
			guiBeginClip(state._clientRect, state._clientRect);
			(*state.windowFunction)(state);
			guiEndClip();
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
