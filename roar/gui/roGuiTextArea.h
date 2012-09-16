void guiTextArea(GuiTextAreaState& state, const roUtf8* text)
{
	state._deducedRect = state.rect;
	_updateWigetState(state);
	_setContentRect(state, guiSkin.textArea, 0, 0);

	guiBeginScrollPanel(state);
		GuiWigetState labelState;
		Rectf textRect = _calTextRect(Rectf(), text);
		_setContentRect(labelState, guiSkin.textArea, textRect.w, textRect.h);

		Canvas& c = *_states.canvas;
		float padding = guiSkin.textArea.padding;

		c.setTextAlign("left");
		c.setTextBaseline("top");
		c.fillText(text, textRect.x + padding, textRect.y + padding, -1);
	guiEndScrollPanel();
}
