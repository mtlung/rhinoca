GuiButtonState::GuiButtonState()
{
}

bool guiButton(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	bool clicked = guiButtonLogic(state, text, style);

	const Rectf& rect = state._deducedRect;
	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, *style);

	// Draw the background
	roRDriverTexture* tex = _selectBackgroundTexture(state, *style);
	c.setGlobalColor(sStyle.backgroundColor.data);
	_draw3x3(tex, rect, style->border);

	// Draw the text
	c.setGlobalColor(sStyle.textColor.data);
	float buttonDownOffset = state.isActive ? 1.0f : 0;
	c.fillText(text, rect.centerx(), rect.centery() + buttonDownOffset, -1);

	return clicked;
}

bool guiButtonLogic(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	Rectf textRect = _calTextRect(Rectf(), text);
	_setContentRect(state, *style, textRect.w, textRect.h);
	_updateWigetState(state);

	return state.isHover && state.isActive && _states.mouseUp();
}
