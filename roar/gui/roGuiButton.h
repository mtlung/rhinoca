GuiButtonState::GuiButtonState()
{
}

bool guiButton(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	bool clicked = guiButtonLogic(state, text, style);
	guiButtonDraw(state, text, style);
	return clicked;
}

void guiButtonDraw(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	const Rectf& rect = state._deducedRect;
	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, *style);

	// Draw the background
	roRDriverTexture* tex = _selectBackgroundTexture(state, *style);
	c.setGlobalColor(sStyle.backgroundColor.data);
	if(sStyle.backgroundColor.a > 0 && !tex) {
		c.setFillColor(1, 1, 1, 1);
		c.fillRect(rect.x, rect.y, rect.w, rect.h);
	}
	else if(tex) {
		c.setFillColor(0, 0, 0, 0);
		_draw3x3(tex, rect, style->border);
	}

	// Draw the text
	if(text && *text != '\0') {
		c.setGlobalColor(sStyle.textColor.data);
		float buttonDownOffset = state.isActive ? 1.0f : 0;
		c.fillText(text, rect.centerx(), rect.centery() + buttonDownOffset, -1);
	}
}

bool guiButtonLogic(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	Sizef textExtend = _calTextExtend(text);
	_setContentExtend(state, *style, textExtend);
	_updateWigetState(state);

	return state.isClicked;
}
