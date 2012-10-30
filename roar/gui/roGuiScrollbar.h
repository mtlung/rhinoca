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

	Canvas& c = *_states.canvas;
	const Rectf& rect = state.rect;

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[1]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, (float)texBg->width, rect.h,
		rect.x, rect.y, rect.w, rect.h
	);

	// The up button
	guiButtonDraw(state.arrowButton1, NULL, &guiSkin.vScrollbarUpButton);

	// The down button
	guiButtonDraw(state.arrowButton2, NULL, &guiSkin.vScrollbarDownButton);

	// The bar
	guiButtonDraw(state.barButton, NULL, &guiSkin.vScrollbarThumbButton);
}

void guiHScrollBar(GuiScrollBarState& state)
{
	guiHScrollBarLogic(state);

	Canvas& c = *_states.canvas;
	const Rectf& rect = state.rect;

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[5]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, rect.w, (float)texBg->height,
		rect.x, rect.y, rect.w, rect.h
	);

	// The up button
	guiButtonDraw(state.arrowButton1, NULL, &guiSkin.hScrollbarLeftButton);

	// The right button
	guiButtonDraw(state.arrowButton2, NULL, &guiSkin.hScrollbarRightButton);

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
	Rectf rectBg(rect.x, rect.y + rectButU.h, rect.w, rect.h - rectButU.h - rectButD.h);
	if(!state.barButton.isHover && (_isClickedUp(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClicky() < rect.centery())
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	guiButtonLogic(state.barButton);
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
	state.barButton.isHover = _isHover(state.barButton.rect);
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
	Rectf rectBg(rect.x + rectButL.w, rect.y, rect.w - rectButL.w - rectButR.w, rect.h);
	if(!state.barButton.isHover && (_isClickedUp(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClickx() < rect.centerx())
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	guiButtonLogic(state.barButton);
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
	state.barButton.isHover = _isHover(state.barButton.rect);
}
