bool guiCheckBox(Rectf rect, const roUtf8* text, bool& state)
{
	if(!text) text = "";

	Rectf textRect = _calTextRect(Rectf(rect.x, rect.y), text);
	Rectf contentRect = textRect;
	contentRect.w += _states.skin.texCheckboxSize.x + _states.margin;
	contentRect.h = roMaxOf2(contentRect.h, _states.skin.texCheckboxSize.y);
	rect = _calMarginRect(rect, contentRect);
	_mergeExtend(_states.panelStateStack.back()->_virtualRect, rect);

	bool hover = _isHover(rect);
	bool hot = _isHot(rect);

	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	// Draw the box
	roRDriverTexture* tex = _states.skin.texCheckbox[(hover ? 2 : 0) + (state ? 1 : 0)]->handle;
	c.drawImage(
		tex, 0, 0, _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y,
		_round(rect.x + _states.margin), _round(rect.centery() - _states.skin.texCheckboxSize.y / 2), _states.skin.texCheckboxSize.x, _states.skin.texCheckboxSize.y
	);

	// Draw the text
	float buttonDownOffset = hot ? 1.0f : 0;
	c.fillText(text, rect.centerx() + _states.skin.texCheckboxSize.x / 2 + _states.margin / 2, rect.centery() + buttonDownOffset, -1);

	GuiButtonState buttonState;
	buttonState.rect = rect;
	bool clicked = guiButtonLogic(buttonState);
	if(clicked)
		state = !state;

	return clicked;
}
