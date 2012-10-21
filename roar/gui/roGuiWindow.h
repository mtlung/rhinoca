GuiWindowState::GuiWindowState()
{
	userData = NULL;
	windowFunction = NULL;
}

void guiWindow(GuiWindowState& state, const GuiStyle* style)
{
	if(!style) style = &guiSkin.panel;
	Canvas& c = *_states.canvas;

	state.deducedRect = state.rect;
	_states.windowList.pushBack(&state);
}
