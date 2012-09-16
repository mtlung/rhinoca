GuiButtonState::GuiButtonState()
{
}

void _drawButton(const Rectf& rect, const roUtf8* text, bool enabled, bool hover, bool down)
{
	Canvas& c = *_states.canvas;
	c.setGlobalColor(1, 1, 1, 1);

	roRDriverTexture* tex = _states.skin.texButton[hover ? 1 : 0]->handle;

	_draw3x3(tex, rect, 3);

	float buttonDownOffset = down ? 1.0f : 0;
	c.fillText(text, rect.centerx(), rect.centery() + buttonDownOffset, -1);
}

bool guiButton(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";

	Rectf rect = state.rect;
	Rectf textRect = _calTextRect(Rectf(rect.x, rect.y), text);
	rect = _calMarginRect(rect, textRect);
	_mergeExtend(_states.panelStateStack.back()->_virtualRect, rect);

	bool clicked = guiButtonLogic(state);

	_drawButton(rect, text, state.isEnable, state.isHover, _isHot(rect));

	return clicked;
}

bool guiButtonLogic(GuiButtonState& state)
{
	state.isHover = _isHover(state.rect);
	bool hot = _isHot(state.rect);

	if(hot)
		_states.potentialHotObject = &state;

	return state.isHover && hot && _states.mouseUp();
}
