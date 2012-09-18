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

	guiDrawBox(state, text, *style, true, true);
}

bool guiButtonLogic(GuiButtonState& state, const roUtf8* text, const GuiStyle* style)
{
	if(!text) text = "";
	if(!style) style = &guiSkin.button;

	Sizef textExtend = _calTextExtend(text);
	_setContentExtend(state, *style, textExtend);
	guiDoLayout(state._deducedRect, style->margin);
	_updateWigetState(state);

	return state.isClicked;
}
