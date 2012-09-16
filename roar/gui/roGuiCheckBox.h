bool guiCheckBox(const Rectf& rect_, const roUtf8* text, bool& checkState)
{
	if(!text) text = "";

	GuiWigetState state;
	state.rect = rect_;
	float padding = guiSkin.checkBox.padding;
	Rectf textRect = _calTextRect(Rectf(), text);
	_setContentRect(state, guiSkin.checkBox,
		textRect.w + padding + _states.skin.texCheckboxSize.x,
		textRect.h
	);
	_updateWigetState(state);
	Rectf& rect = state._deducedRect;

	Canvas& c = *_states.canvas;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(state, guiSkin.checkBox);

	// Draw the box
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

	GuiButtonState buttonState;
	buttonState.rect = rect;
	bool clicked = guiButtonLogic(buttonState);
	if(clicked)
		checkState = !checkState;

	return clicked;
}
