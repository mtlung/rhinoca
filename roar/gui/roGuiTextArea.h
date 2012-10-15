GuiTextAreaState::GuiTextAreaState()
{
	highLightBegPos = 0;
	highLightEndPos = 0;
}

static void _removeHighLightText(GuiTextAreaState& state, String& text, bool backSpace, bool del)
{
	roAssert(!(backSpace && del) && "back space and delete should not be both true");

	roSize& posBeg = state.highLightBegPos;
	roSize& posEnd = state.highLightEndPos;

	if(text.isEmpty()) {
		posBeg = posEnd = 0;
		return;
	}

	roSize minRemoveCount = (backSpace || del) ? 1 : 0;
	roSize removeCount = roMinOf2(minRemoveCount, posEnd - posBeg);
	text.erase(posBeg, removeCount);

	if(posBeg == posEnd) {
		roAssert(minRemoveCount == 1);
		posBeg = roClampedSubtraction(posBeg, minRemoveCount);
	}

	posEnd = posBeg;
}

void guiTextArea(GuiTextAreaState& state, String& text)
{
	state.deducedRect = state.rect;
	_setContentExtend(state, guiSkin.textArea, Sizef());
	_updateWigetState(state);

	guiBeginScrollPanel(state);
		GuiWigetState labelState;
		Sizef textExtend = _calTextExtend(text.c_str());
		_setContentExtend(labelState, guiSkin.textArea, textExtend);
		_updateWigetState(labelState);

		Canvas& c = *_states.canvas;
		float padding = guiSkin.textArea.padding;
		const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(labelState, guiSkin.textArea);

		roInputDriver* inputDriver = roSubSystems->inputDriver;
		text += inputDriver->inputText(inputDriver);

		roSize& posBeg = state.highLightBegPos;
		roSize& posEnd = state.highLightEndPos;

		// Handle character removal
		if(!text.isEmpty())
		{
			if(inputDriver->buttonDown(inputDriver, stringHash("Back"), false))
				_removeHighLightText(state, text, true, false);

			if(inputDriver->buttonDown(inputDriver, stringHash("Delete"), false))
				_removeHighLightText(state, text, false, true);

			if(inputDriver->buttonDown(inputDriver, stringHash("Return"), false))
				text.append('\n');
		}
		else
			posBeg = posEnd = 0;

		{	// Handle keyboard input to manipulate the carte position
			if(inputDriver->buttonDown(inputDriver, stringHash("Left"), false))
				posBeg = posEnd = roClampedSubtraction(posBeg, 1u);

			if(inputDriver->buttonDown(inputDriver, stringHash("Right"), false))
				posBeg = posEnd = posEnd + 1;

			// Make sure the range is correct
			posBeg = roClamp(posBeg, 0u, text.size());
			posEnd = roClamp(posEnd, 0u, text.size());
		}

		c.setFont("10pt \"DFKai-SB\"");
		c.setTextAlign("left");
		c.setTextBaseline("top");
		c.setGlobalColor(sStyle.textColor.data);
		c.fillText(text.c_str(), padding, padding, -1);
	guiEndScrollPanel();
}
