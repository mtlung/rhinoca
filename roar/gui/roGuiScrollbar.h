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

	c.setGlobalColor(1, 1, 1, 1);

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[1]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, (float)texBg->width, rect.h,
		rect.x, rect.y, rect.w, rect.h
	);

	// The up button
	Rectf& rectBut1 = state.arrowButton1.rect;
	bool isUpButtonHot = _isHot(rectBut1);
	roRDriverTexture* texBut = _states.skin.texScrollPanel[isUpButtonHot ? 4 : 3]->handle;
	c.drawImage(
		texBut,
		0, 0, (float)texBut->width, texBut->height / 2.f,
		rectBut1.x, rectBut1.y, rectBut1.w, rectBut1.h
	);

	// The down button
	Rectf& rectBut2 = state.arrowButton2.rect;
	bool isDownButtonHot = _isHot(rectBut2);
	texBut = _states.skin.texScrollPanel[isDownButtonHot ? 4 : 3]->handle;
	c.drawImage(
		texBut,
		0, texBut->height / 2.f, (float)texBut->width, texBut->height / 2.f,
		rectBut2.x, rectBut2.y, rectBut2.w, rectBut2.h
	);

	// The bar
	roRDriverTexture* texBar = _states.skin.texScrollPanel[2]->handle;
	_draw3x3(texBar, state.barButton.rect, 2);
}

void guiHScrollBar(GuiScrollBarState& state)
{
	guiHScrollBarLogic(state);

	Canvas& c = *_states.canvas;
	const Rectf& rect = state.rect;

	c.setGlobalColor(1, 1, 1, 1);

	// Background
	roRDriverTexture* texBg = _states.skin.texScrollPanel[5]->handle;
	c.drawImage(
		texBg,
		rect.x, rect.y, rect.w, (float)texBg->height,
		rect.x, rect.y, rect.w, rect.h
	);

	// The left button
	Rectf& rectBut1 = state.arrowButton1.rect;
	bool isLeftButtonHot = _isHot(rectBut1);
	roRDriverTexture* texBut = _states.skin.texScrollPanel[isLeftButtonHot ? 8 : 7]->handle;
	c.drawImage(
		texBut,
		0, 0, texBut->width / 2.f, (float)texBut->height,
		rectBut1.x, rectBut1.y, rectBut1.w, rectBut1.h
	);

	// The right button
	Rectf& rectBut2 = state.arrowButton2.rect;
	bool isRightButtonHot = _isHot(rectBut2);
	texBut = _states.skin.texScrollPanel[isRightButtonHot ? 8 : 7]->handle;
	c.drawImage(
		texBut,
		texBut->width / 2.f, 0, texBut->width / 2.f, (float)texBut->height,
		rectBut2.x, rectBut2.y, rectBut2.w, rectBut2.h
	);

	// The bar
	roRDriverTexture* texBar = _states.skin.texScrollPanel[6]->handle;
	_draw3x3(texBar, state.barButton.rect, 2);
}

void guiVScrollBarLogic(GuiScrollBarState& state)
{
	const Rectf& rect = state.rect;
	Rectf& rectButU = state.arrowButton1.rect;
	Rectf& rectButD = state.arrowButton2.rect;
	float slideSize = rect.h - rectButU.h - rectButD.h;
	float barSize = roMaxOf2((state._pageSize * slideSize) / (state.valueMax + state._pageSize), 10.f);

	// Update buttons
	roRDriverTexture* texBut = _states.skin.texScrollPanel[3]->handle;

	rectButU = Rectf(rect.x, rect.y, rect.w, texBut->height / 2.f);
	state.arrowButton1.isHover = _isHover(rectButU);

	rectButD = Rectf(rect.x, rect.bottom() - texBut->height / 2.f, rect.w, texBut->height / 2.f);
	state.arrowButton2.isHover = _isHover(rectButD);

	// Handle arrow button click
	if(guiButtonLogic(state.arrowButton1) || _isRepeatedClick(rectButU))
		state.value -= state.smallStep;
	if(guiButtonLogic(state.arrowButton2) || _isRepeatedClick(rectButD))
		state.value += state.smallStep;

	// Handle bar background click
	Rectf rectBg(rect.x, rect.y + rectButU.h, rect.w, rect.h - rectButU.h - rectButD.h);
	if(_states.lastFrameHotObject != &state.barButton && (_isClicked(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClicky() < rect.centery())
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	guiButtonLogic(state.barButton);
	if(_states.hotObject == &state.barButton)
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
	roRDriverTexture* texBut = _states.skin.texScrollPanel[7]->handle;

	rectButL = Rectf(rect.x, rect.y, texBut->width / 2.f, rect.h);
	state.arrowButton1.isHover = _isHover(rectButL);

	rectButR = Rectf(rect.right() - texBut->width / 2.f, rect.y, texBut->width / 2.f, rect.h);
	state.arrowButton2.isHover = _isHover(rectButR);

	// Handle arrow button click
	if(guiButtonLogic(state.arrowButton1) || _isRepeatedClick(rectButL))
		state.value -= state.smallStep;
	if(guiButtonLogic(state.arrowButton2) || _isRepeatedClick(rectButR))
		state.value += state.smallStep;

	// Handle bar background click
	Rectf rectBg(rect.x + rectButL.w, rect.y, rect.w - rectButL.w - rectButR.w, rect.h);
	if(_states.lastFrameHotObject != &state.barButton && (_isClicked(rectBg) || _isRepeatedClick(rectBg)))
	{
		if(_states.mouseClickx() < rect.centerx())
			state.value -= state.largeStep;
		else
			state.value += state.largeStep;
	}

	// Handle bar button drag
	guiButtonLogic(state.barButton);
	if(_states.hotObject == &state.barButton)
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
