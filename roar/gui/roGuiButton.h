bool guiButton(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	bool clicked = guiButtonLogic(state, text, style);
	guiButtonDraw(state, text, style);
	return clicked;
}

bool guiToggleButton(GuiButtonState& state, bool& onOffState, const roUtf8* text, const GuiStyle* onStyle, const GuiStyle* offStyle)
{
	if(!text) text = "";
	const GuiStyle* style = onOffState ? onStyle : offStyle;
	bool clicked = guiButtonLogic(state, text, style);

	if(clicked)
		onOffState = !onOffState;

	style = onOffState ? onStyle : offStyle;
	guiButtonDraw(state, text, style);

	return onOffState;
}

void guiButtonDraw(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	guiDrawBox(state, text, *style, true, true);
}

bool guiButtonLogic(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	Sizef textExtend = _calTextExtend(text);
	_setContentExtend(state, *style, textExtend);
	guiDoLayout(state.rect, state.deducedRect, style->margin);
	_updateWigetState(state);

	return state.isClicked;
}
