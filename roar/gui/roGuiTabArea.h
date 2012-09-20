GuiTabAreaState::GuiTabAreaState()
{
	activeTabIndex = 0;
	_tabCount = 0;
}

void guiBeginTabs(GuiTabAreaState& state)
{
	state._tabCount = 0;

	_setContentExtend(state, guiSkin.tabArea, Sizef());
	_updateWigetState(state);
	guiPushHostWiget(state);

	state.clientPanel.rect.x = state.rect.x;
	state.clientPanel.rect.y = state.rect.y + 40;
	state.clientPanel.rect.w = state.rect.w;
	state.clientPanel.rect.h = state.rect.h - 40;
	guiBeginScrollPanel(state.clientPanel);
}

void guiEndTabs()
{
	guiEndScrollPanel();

	GuiTabAreaState& state = *static_cast<GuiTabAreaState*>(guiGetHostWiget());

	state.tabButtons.rect.w = 200;
	guiBeginScrollPanel(state.tabButtons);
	guiBeginFlowLayout(Rectf(0, 0, 0, 30), 'h');
		GuiButtonState button;
		for(roSize i=0; i<state._tabCount; ++i) {
			String& tabText = guiGetStringFromStack(state._tabCount - i - 1);
			if(guiButton(button, tabText.c_str()))
				state.activeTabIndex = i;
		}
	guiEndFlowLayout();
	guiEndScrollPanel();

	guiPopHostWiget();

}

bool guiBeginTab(const roUtf8* text)
{
	GuiTabAreaState* parent = static_cast<GuiTabAreaState*>(guiGetHostWiget());
	if(!parent) return false;

	bool ret = parent->activeTabIndex == parent->_tabCount;
	parent->_tabCount++;

	guiPushStringToStack(text);

	return ret;
}

void guiEndTab()
{
}
