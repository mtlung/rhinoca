static void _doHorizontalFlowLayout(Rectf& rect, float margin)
{
	float& x = guiGetFloatFromStack(3);
	float& y = guiGetFloatFromStack(2);
	float& h = guiGetFloatFromStack(0);
	rect.x = x + margin;
	rect.y = y + margin;
	rect.h = h - margin * 2;
	x += rect.w + margin;
}

static void _doVerticalFlowLayout(Rectf& rect, float margin)
{
	float& x = guiGetFloatFromStack(3);
	float& y = guiGetFloatFromStack(2);
	float& w = guiGetFloatFromStack(1);
	rect.x = x + margin;
	rect.y = y + margin;
	rect.w = w - margin * 2;
	y += rect.h + margin;
}

void guiBeginFlowLayout(Rectf rect, char hORv)
{
	guiDoLayout(rect, 0);

	if(hORv == 'h' || hORv == 'H')
		guiPushPtrToStack(_doHorizontalFlowLayout);
	else if(hORv == 'v' || hORv == 'V')
		guiPushPtrToStack(_doVerticalFlowLayout);
	else
		guiPushPtrToStack(NULL);

	guiPushFloatToStack(rect.x);
	guiPushFloatToStack(rect.y);
	guiPushFloatToStack(rect.w);
	guiPushFloatToStack(rect.h);
}

void guiEndFlowLayout()
{
	guiPopPtrFromStack();
	guiPopFloatFromStack();
	guiPopFloatFromStack();
	guiPopFloatFromStack();
	guiPopFloatFromStack();
}
