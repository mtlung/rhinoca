static void _doHorizontalFlowLayout(const Rectf& rect, Rectf& deducedRect, float margin)
{
	float& x = guiGetFloatFromStack(3);
	float& y = guiGetFloatFromStack(2);
	float& h = guiGetFloatFromStack(0);
	deducedRect.x = x + margin;
	deducedRect.y = y + margin;
	deducedRect.h = roMaxOf2(h - margin * 2, deducedRect.h);
	x += deducedRect.w + margin;
}

static void _doVerticalFlowLayout(const Rectf& rect, Rectf& deducedRect, float margin)
{
	float& x = guiGetFloatFromStack(3);
	float& y = guiGetFloatFromStack(2);
	float& w = guiGetFloatFromStack(1);
	deducedRect.x = x + margin;
	deducedRect.y = y + margin;
	deducedRect.w = roMaxOf2(w - margin * 2, deducedRect.w);
	y += deducedRect.h + margin;
}

void guiBeginFlowLayout(Rectf rect, char hORv)
{
	guiDoLayout(rect, rect, 0);

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
