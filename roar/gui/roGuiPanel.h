GuiPanelState::GuiPanelState()
{
	showBorder = true;
	scrollable = true;
}

void guiBeginScrollPanel(GuiPanelState& state, const GuiStyle* style)
{
	if(!style) style = &guiSkin.panel;

	float border = style->border;
	float padding = style->padding;
	guiPushFloatToStack(border);
	guiPushFloatToStack(padding);

	const Rectf& rect = state.rect;
	const Rectf& deducedRect = state.deducedRect;
	const Rectf& clientRect = state.clientRect;
	Sizef deducedSize = Sizef(rect.w == 0 ? state._virtualRect.w - 2 * style->padding : 0, rect.h == 0 ? state._virtualRect.h - 2 * style->padding : 0);

	_setContentExtend(state, *style, deducedSize);
	_updateWigetState(state);
	_states.panelStateStack.pushBack(&state);

	if(state.clientRect.w == 0 || state.clientRect.h == 0)
		state.clientRect = deducedRect;

	if(_states.mouseDown()) {
		state.mouseClickPos.x = _states.mousex() - state._virtualRect.x;
		state.mouseClickPos.y = _states.mousey() - state._virtualRect.y;
	}
	_states.mouseClickPosStack.pushBack(state.mouseClickPos);

	// Try capture the mouse and prevent the mouse scroll fall though this panel
	if(_isHover(deducedRect))
		_states.mouseCapturedObject = &state;

	// Detect mouse scroll
	if(_states.lastFrameMouseCapturedObject == &state && state.scrollable)
		state.vScrollBar.value -= _states.mousez() * 10;

	state.vScrollBar.value = roClamp(state.vScrollBar.value, 0.f, state.vScrollBar.valueMax);
	state.hScrollBar.value = roClamp(state.hScrollBar.value, 0.f, state.hScrollBar.valueMax);

	// Initialize the virtual bound
	Rectf& virtualRect = state._virtualRect;
	virtualRect.x = clientRect.x + padding - state.hScrollBar.value;
	virtualRect.y = clientRect.y + padding - state.vScrollBar.value;
	virtualRect.w = 0;
	virtualRect.h = 0;

	// Draw background
	GuiWigetState buttonState = state;
	guiDrawBox(buttonState, NULL, *style, state.showBorder, true);

	guiBeginClip(state.clientRect, virtualRect);
}

void guiEndScrollPanel()
{
	GuiPanelState& panelState = *_states.panelStateStack.back();

	guiEndClip();

	_states.mouseCaptured = false;

	float padding = guiPopFloatFromStack();
	float border = guiPopFloatFromStack();

	const Rectf& rect = panelState.deducedRect;
	Rectf& virtualRect = panelState._virtualRect;

	virtualRect.w += padding * 2;
	virtualRect.h += padding * 2;

	// Determine whether we need to show scroll bars
	bool showVScrollBar = false;
	bool showHScrollBar = false;
	float vScrollBarThickness = (float)guiSkin.vScrollbarThumbButton.normal.backgroundImage->width();
	float hScrollBarThickness = (float)guiSkin.hScrollbarThumbButton.normal.backgroundImage->height();
	if(panelState.scrollable && (virtualRect.w > rect.w || virtualRect.h > rect.h)) {
		showVScrollBar = virtualRect.h > rect.h;
		showHScrollBar = virtualRect.w > (rect.w - vScrollBarThickness);
	}

	// Update the client area
	Rectf& clientRect = panelState.clientRect;
	clientRect = panelState.deducedRect;
	clientRect.x += panelState.showBorder ? border : 0;
	clientRect.w -= panelState.showBorder ? border * 2 : 0;
	clientRect.y += panelState.showBorder ? border : 0;
	clientRect.h -= panelState.showBorder ? border * 2 : 0;
	clientRect.w -= showVScrollBar ? vScrollBarThickness : 0;
	clientRect.h -= showHScrollBar ? hScrollBarThickness : 0;

	if(panelState.scrollable)
	{
		// Draw the vertical scroll bar
		panelState.vScrollBar.rect = Rectf(
			rect.right() - vScrollBarThickness - border,
			rect.y + border,
			vScrollBarThickness,
			rect.h - border * 2
		);
		panelState.vScrollBar._pageSize = (rect.h - border * 2) - (showHScrollBar ? hScrollBarThickness : 0);
		panelState.vScrollBar.valueMax = roMaxOf2(virtualRect.h - panelState.vScrollBar._pageSize, 0.f);
		if(showVScrollBar)
			guiVScrollBar(panelState.vScrollBar);

		// Draw the horizontal scroll bar
		panelState.hScrollBar.rect = Rectf(
			rect.x + border,
			rect.bottom() - hScrollBarThickness - border,
			rect.w - border * 2 - (showVScrollBar ? hScrollBarThickness : 0),
			hScrollBarThickness
		);
		panelState.hScrollBar._pageSize = (rect.w - border * 2) - (showVScrollBar ? vScrollBarThickness : 0);
		panelState.hScrollBar.valueMax = roMaxOf2(virtualRect.w - panelState.hScrollBar._pageSize, 0.f);
		if(showHScrollBar)
			guiHScrollBar(panelState.hScrollBar);
	}

	_states.panelStateStack.popBack();
}
