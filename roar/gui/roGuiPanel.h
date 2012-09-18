GuiPanelState::GuiPanelState()
{
	showBorder = true;
	scrollable = true;
}

void guiBeginScrollPanel(GuiPanelState& state, const GuiStyle* style)
{
	if(!style) style = &guiSkin.panel;
	Canvas& c = *_states.canvas;

	const Rectf& rect = state.rect;

	_setContentExtend(state, *style, Sizef());
	_updateWigetState(state);
	_states.panelStateStack.pushBack(&state);

	if(state._clientRect.w == 0 || state._clientRect.h == 0)
		state._clientRect = state.rect;

	// Detect mouse scroll
	if(_states.lastFrameHoveringObject == &state && state.scrollable)
		state.vScrollBar.value -= _states.mousez() * 10;

	state.vScrollBar.value = roClamp(state.vScrollBar.value, 0.f, state.vScrollBar.valueMax);
	state.hScrollBar.value = roClamp(state.hScrollBar.value, 0.f, state.hScrollBar.valueMax);

	// Initialize the virtual bound
	Rectf& virtualRect = state._virtualRect;
	virtualRect.x = rect.x - state.hScrollBar.value;
	virtualRect.y = rect.y - state.vScrollBar.value;
	virtualRect.w = 0;
	virtualRect.h = 0;

	// Draw background
	GuiWigetState buttonState = state;
	guiDrawBox(buttonState, NULL, *style, state.showBorder, true);

	guiBeginClip(state._clientRect);

	// NOTE: Truncate float value to integer, so we will always have pixel perfect match
	c.translate(_round(virtualRect.x), _round(virtualRect.y));

	_states.offsetx -= virtualRect.x;
	_states.offsety -= virtualRect.y;
}

void guiEndScrollPanel()
{
	GuiPanelState& panelState = *_states.panelStateStack.back();

	guiEndClip();

	_states.mouseCaptured = false;

	float border = 2;
	const Rectf& rect = panelState.rect;
	Rectf& virtualRect = panelState._virtualRect;

	_states.offsetx += virtualRect.x;
	_states.offsety += virtualRect.y;

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
	Rectf& clientRect = panelState._clientRect;
	clientRect = rect;
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
