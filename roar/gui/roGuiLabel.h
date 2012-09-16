void guiLabel(const Rectf& rect, const roUtf8* text)
{
	if(!text) text = "";

	GuiWigetState state;
	Rectf textRect = _calTextRect(Rectf(), text);
	_setContentRect(state, guiSkin.label, textRect.w, textRect.h);
	_updateWigetState(state);

	_states.canvas->fillText(text, state._deducedRect.centerx(), state._deducedRect.centery(), -1);
}
