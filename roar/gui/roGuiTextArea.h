void guiTextArea(GuiTextAreaState& state, const roUtf8* text)
{
	state.deducedRect = state.rect;
	_setContentExtend(state, guiSkin.textArea, Sizef());
	_updateWigetState(state);

	guiBeginScrollPanel(state);
		GuiWigetState labelState;
		Sizef textExtend = _calTextExtend(text);
		_setContentExtend(labelState, guiSkin.textArea, textExtend);
		_updateWigetState(labelState);

		Canvas& c = *_states.canvas;
		float padding = guiSkin.textArea.padding;
		const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(labelState, guiSkin.textArea);

		c.setTextAlign("left");
		c.setTextBaseline("top");
		c.setGlobalColor(sStyle.textColor.data);
		c.fillText(text, padding, padding, -1);
	guiEndScrollPanel();
}
