GuiTextAreaState::GuiTextAreaState()
{
	highLightBegPos = 0;
	highLightEndPos = 0;
}

void guiTextArea(GuiTextAreaState& state, String& text)
{
	state.deducedRect = state.rect;
	_setContentExtend(state, guiSkin.textArea, Sizef());
	_updateWigetState(state);

	guiBeginScrollPanel(state);
		GuiWigetState labelState;
		Sizef textExtend = _calTextExtend(text.c_str());
		_setContentExtend(labelState, guiSkin.textArea, textExtend);
		_updateWigetState(labelState);

		Canvas& c = *_states.canvas;
		float padding = guiSkin.textArea.padding;
		const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(labelState, guiSkin.textArea);

		roInputDriver* inputDriver = roSubSystems->inputDriver;
		text += inputDriver->inputText(inputDriver);

		c.setFont("10pt \"DFKai-SB\"");
		c.setTextAlign("left");
		c.setTextBaseline("top");
		c.setGlobalColor(sStyle.textColor.data);
		c.fillText(text.c_str(), padding, padding, -1);
	guiEndScrollPanel();
}
