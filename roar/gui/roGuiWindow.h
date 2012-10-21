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

		guiDrawBox(state, NULL, guiSkin.panel, true, true);

		if(state.windowFunction) {
			guiBeginClip(state.deducedRect);
			c.translate(_round(state.deducedRect.x), _round(state.deducedRect.y));

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
