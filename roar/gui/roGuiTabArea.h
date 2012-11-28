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

	state.clientPanel.rect.x = state.deducedRect.x;
	state.clientPanel.rect.y = state.deducedRect.y + state.tabButtons.deducedRect.h;
	state.clientPanel.rect.w = state.deducedRect.w;
	state.clientPanel.rect.h = state.deducedRect.h - state.tabButtons.deducedRect.h;
	guiBeginScrollPanel(state.clientPanel, &guiSkin.tabArea);
}

void guiEndTabs()
{
	guiEndScrollPanel();

	GuiTabAreaState& state = *static_cast<GuiTabAreaState*>(guiGetHostWiget());

	GuiStyle style;
	guiBeginScrollPanel(state.tabButtons, &style);
	guiBeginFlowLayout(Rectf(), 'h');
		GuiButtonState button;
		for(roSize i=0; i<state._tabCount; ++i) {
			String& tabText = guiGetStringFromStack(state._tabCount - i - 1);
			if(guiButton(button, tabText.c_str()))
				state.activeTabIndex = i;
		}
	guiEndFlowLayout();
	guiEndScrollPanel();

	guiPopHostWiget();

	for(roSize i=0; i<state._tabCount; ++i)
		guiPopStringFromStack();
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
