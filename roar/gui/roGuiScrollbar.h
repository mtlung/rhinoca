GuiScrollBarState::GuiScrollBarState()
{
	_pageSize = 0;
	value = 0;
	valueMax = 0;
	smallStep = 5.f;
	largeStep = smallStep * 5;
}

void guiVScrollBar(GuiScrollBarState& state)
{
	guiVScrollBarLogic(state);

	// The up/down buttons
	guiButtonDraw(state.arrowButton1, NULL, &guiSkin.vScrollbarUpButton);
	guiButtonDraw(state.arrowButton2, NULL, &guiSkin.vScrollbarDownButton);

	// The background
	guiButtonDraw(state.bgButton1, NULL, &guiSkin.vScrollbarBG);
	guiButtonDraw(state.bgButton2, NULL, &guiSkin.vScrollbarBG);

	// The bar
	guiButtonDraw(state.barButton, NULL, &guiSkin.vScrollbarThumbButton);
}

void guiHScrollBar(GuiScrollBarState& state)
{
	guiHScrollBarLogic(state);

	// The left/right buttons
	guiButtonDraw(state.arrowButton1, NULL, &guiSkin.hScrollbarLeftButton);
	guiButtonDraw(state.arrowButton2, NULL, &guiSkin.hScrollbarRightButton);

	// The background
	guiButtonDraw(state.bgButton1, NULL, &guiSkin.hScrollbarBG);
	guiButtonDraw(state.bgButton2, NULL, &guiSkin.hScrollbarBG);

	// The bar
	guiButtonDraw(state.barButton, NULL, &guiSkin.hScrollbarThumbButton);
}

void guiVScrollBarLogic(GuiScrollBarState& state)
{
	const Rectf& rect = state.rect;
	Rectf& rectButU = state.arrowButton1.rect;
	Rectf& rectButD = state.arrowButton2.rect;
	float slideSize = rect.h - rectButU.h - rectButD.h;
	float barSize = roMaxOf2((state._pageSize * slideSize) / (state.valueMax + state._pageSize), 10.f);

	// Update buttons
	float upButtonHeight = (float)guiSkin.vScrollbarUpButton.normal.backgroundImage->height();
	rectButU = Rectf(rect.x, rect.y, rect.w, upButtonHeight);
	guiButtonLogic(state.arrowButton1, NULL, &guiSkin.vScrollbarUpButton);

	float downButtonHeight = (float)guiSkin.vScrollbarDownButton.normal.backgroundImage->height();
	rectButD = Rectf(rect.x, rect.bottom() - downButtonHeight, rect.w, downButtonHeight);
	guiButtonLogic(state.arrowButton2, NULL, &guiSkin.vScrollbarDownButton);

	// Handle arrow button click
	if(state.arrowButton1.isClicked || state.arrowButton1.isClickRepeated)
		state.value -= state.smallStep;
	if(state.arrowButton2.isClicked || state.arrowButton2.isClickRepeated)
		state.value += state.smallStep;

	// Handle bar background click
	if(state.bgButton1.isClicked || state.bgButton1.isClickRepeated)
		state.value -= state.largeStep;
	if(state.bgButton2.isClicked || state.bgButton2.isClickRepeated)
		state.value += state.largeStep;

	// Handle bar button drag
	if(state.barButton.isActive)
		state.value += (_states.mousedy() * state.valueMax / (slideSize - barSize));

	state.value = roClamp(state.value, 0.f, state.valueMax);

	// Update bar rect
    float barPos = ((slideSize - barSize) * state.value) / state.valueMax;
	state.barButton.rect = Rectf(
		rect.x,
		rect.y + rectButU.h + barPos,
		rect.w,
		barSize
	);
	guiButtonLogic(state.barButton, NULL, &guiSkin.vScrollbarThumbButton);

	// Update background rect
	state.bgButton1.rect = Rectf(
		rect.x,
		rectButU.bottom(),
		rect.w,
		state.barButton.rect.top() - rectButU.bottom()
	);
	guiButtonLogic(state.bgButton1, NULL, &guiSkin.vScrollbarBG);
	state.bgButton2.rect = Rectf(
		rect.x,
		state.barButton.rect.bottom(),
		rect.w,
		rectButD.top() - state.barButton.rect.bottom()
	);
	guiButtonLogic(state.bgButton2, NULL, &guiSkin.vScrollbarBG);
}

void guiHScrollBarLogic(GuiScrollBarState& state)
{
	const Rectf& rect = state.rect;
	Rectf& rectButL = state.arrowButton1.rect;
	Rectf& rectButR = state.arrowButton2.rect;
	float slideSize = rect.w - rectButL.w - rectButR.w;
	float barSize = roMaxOf2((state._pageSize * slideSize) / (state.valueMax + state._pageSize), 10.f);

	// Update buttons
	float leftButtonWidth = (float)guiSkin.hScrollbarLeftButton.normal.backgroundImage->width();
	rectButL = Rectf(rect.x, rect.y, leftButtonWidth, rect.h);
	guiButtonLogic(state.arrowButton1, NULL, &guiSkin.hScrollbarLeftButton);

	float rightButtonWidth = (float)guiSkin.hScrollbarRightButton.normal.backgroundImage->width();
	rectButR = Rectf(rect.right() - rightButtonWidth, rect.y, rightButtonWidth, rect.h);
	guiButtonLogic(state.arrowButton2, NULL, &guiSkin.hScrollbarRightButton);

	// Handle arrow button click
	if(state.arrowButton1.isClicked || state.arrowButton1.isClickRepeated)
		state.value -= state.smallStep;
	if(state.arrowButton2.isClicked || state.arrowButton2.isClickRepeated)
		state.value += state.smallStep;

	// Handle bar background click
	if(state.bgButton1.isClicked || state.bgButton1.isClickRepeated)
		state.value -= state.largeStep;
	if(state.bgButton2.isClicked || state.bgButton2.isClickRepeated)
		state.value += state.largeStep;

	// Handle bar button drag
	if(state.barButton.isActive)
		state.value += (_states.mousedx() * state.valueMax / (slideSize - barSize));

	state.value = roClamp(state.value, 0.f, state.valueMax);

	// Update bar rect
	float barPos = ((slideSize - barSize) * state.value) / state.valueMax;
	state.barButton.rect = Rectf(
		rect.x + rectButL.w + barPos,
		rect.y,
		barSize,
		rect.h
	);
	guiButtonLogic(state.barButton, NULL, &guiSkin.hScrollbarThumbButton);

	// Update background rect
	state.bgButton1.rect = Rectf(
		rectButL.right(),
		rect.y,
		state.barButton.rect.left() - rectButL.right(),
		rect.h
	);
	guiButtonLogic(state.bgButton1, NULL, &guiSkin.hScrollbarBG);
	state.bgButton2.rect = Rectf(
		state.barButton.rect.right(),
		rect.y,
		rectButR.left() - state.barButton.rect.right(),
		rect.h
	);
	guiButtonLogic(state.bgButton2, NULL, &guiSkin.hScrollbarBG);
}
