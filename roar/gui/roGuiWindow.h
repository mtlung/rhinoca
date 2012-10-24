GuiWindowState::GuiWindowState()
{
	userData = NULL;
	windowFunction = NULL;
}

static void _drawWindows()
{
	Canvas& c = *_states.canvas;

	for(roSize i=0; i<_states.windowList.size(); ++i) {
		roAssert(_states.windowList[i]);
		GuiWindowState& state = *_states.windowList[i];

		// Draw background
		guiDrawBox(state, NULL, guiSkin.window, true, true);

		// Draw title bar
		state.titleBar.rect = state.deducedRect;
		state.titleBar.rect.h = 10;	// This is the minimum bar height

		Sizef textExtend(c.lineSpacing(), c.lineSpacing());
		GuiStyle dummyStyle;
		_setContentExtend(state.titleBar, dummyStyle, textExtend);
		state.titleBar.deducedRect.y -= state.titleBar.deducedRect.h;
		guiButtonDraw(state.titleBar, state.title.c_str(), &guiSkin.windowTitle);

		if(state.windowFunction) {
			guiBeginClip(state.deducedRect, state.deducedRect);
			(*state.windowFunction)(state);
			guiEndClip();
		}
	}

	_states.windowList.clear();
}

void guiWindow(GuiWindowState& state, const GuiStyle* style)
{
	state.deducedRect = state.rect;
	_states.windowList.pushBack(&state);
}
