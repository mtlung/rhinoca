void guiLabel(const Rectf& rect_, const roUtf8* text)
{
	if(!text) text = "";

	GuiWigetState state;
	state.rect = rect_;
	Sizef textExtend = _calTextExtend(text);
	_setContentExtend(state, guiSkin.label, textExtend);
	_updateWigetState(state);

	const Rectf& rect = state.deducedRect;
	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, guiSkin.label);

	// Draw the background
	roRDriverTexture* tex = _selectBackgroundTexture(state, guiSkin.label);
	c.setGlobalColor(sStyle.backgroundColor.data);
	if(sStyle.backgroundColor.a > 0 && !tex) {
		c.setFillColor(1, 1, 1, 1);
		c.fillRect(rect.x, rect.y, rect.w, rect.h);
	}
	else if(tex) {
		c.setFillColor(0, 0, 0, 0);
		_draw3x3(tex, rect, guiSkin.label.border);
	}

	// Draw the text
	c.setGlobalColor(sStyle.textColor.data);
	_states.canvas->fillText(text, state.deducedRect.centerx(), state.deducedRect.centery(), -1);
}
