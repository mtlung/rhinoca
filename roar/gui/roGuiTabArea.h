void guiBeginTabs(GuiTabAreaState& state)
{
	_setContentExtend(state, guiSkin.tabArea, Sizef());
	_updateWigetState(state);
	guiPushHostWiget(state);
	guiBeginClip(state.rect);

}

void guiEndTabs()
{
	GuiTabAreaState& state = *static_cast<GuiTabAreaState*>(guiGetHostWiget());

	guiBeginScrollPanel(state.tabButtons);
	guiLabel(state.rect, "Tab 1");
	guiEndScrollPanel();

	guiPopHostWiget();
	guiEndClip();
}

bool guiBeginTab(const roUtf8* text)
{
	GuiWigetState* tabs = guiGetHostWiget();
	if(!tabs) return false;

	return true;
}

void guiEndTab()
{
}
