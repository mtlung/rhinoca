bool guiCheckBox(const Rectf& rect_, const roUtf8* text, bool& checkState)
{
	if(!text) text = "";

	GuiWigetState state;
	state.rect = rect_;
	float padding = guiSkin.checkBox.padding;
	Sizef textExtend = _calTextExtend(text);
	_setContentExtend(state, guiSkin.checkBox, Sizef(
		textExtend.w + padding + _states.skin.texCheckboxSize.x,
		textExtend.h
	));
	_updateWigetState(state);

	Rectf& rect = state.deducedRect;
	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, guiSkin.checkBox);

	// Draw the background
	roRDriverTexture* bgTex = _selectBackgroundTexture(state, guiSkin.checkBox);
	c.setGlobalColor(sStyle.backgroundColor.data);
	if(sStyle.backgroundColor.a > 0 && !bgTex) {
		c.setFillColor(1, 1, 1, 1);
		c.fillRect(rect.x, rect.y, rect.w, rect.h);
	}
	else if(bgTex) {
		c.setFillColor(0, 0, 0, 0);
		_draw3x3(bgTex, rect, guiSkin.label.border);
	}

	// Draw the tick-box
	c.setGlobalColor(1, 1, 1, 1);
	roRDriverTexture* tex = _states.skin.texCheckbox[(state.isHover ? 2 : 0) + (checkState ? 1 : 0)]->handle;
	c.drawImage(
		tex, 0, 0, _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y,
		_round(rect.x + padding), _round(rect.centery() - _states.skin.texCheckboxSize.y / 2),
		_states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y
	);

	// Draw the text
	c.setGlobalColor(sStyle.textColor.data);
	float buttonDownOffset = state.isActive ? 1.0f : 0;
	c.fillText(text, rect.centerx() + _states.skin.texCheckboxSize.x / 2 + padding / 2, rect.centery() + buttonDownOffset, -1);

	// Logic
	GuiButtonState buttonState;
	buttonState.rect = rect;
	bool clicked = guiButtonLogic(buttonState);
	if(clicked)
		checkState = !checkState;

	return clicked;
}
