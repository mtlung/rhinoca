void guiTextArea(GuiTextAreaState& state, const roUtf8* text)
{
	guiBeginScrollPanel(state);

	Canvas& c = *_states.canvas;

	Rectf rect = state.rect;
	Rectf textRect = _calTextRect(Rectf(rect.x + _states.margin, rect.y + _states.margin), text);
	rect = _calMarginRect(rect, textRect);
	_mergeExtend(_states.panelStateStack.back()->_virtualRect, textRect);

	c.setTextAlign("left");
	c.setTextBaseline("top");
	c.fillText(text, textRect.x, textRect.y, -1);

	guiEndScrollPanel();
}
