GuiTextAreaState::GuiTextAreaState()
{
	highLightBegPos = 0;
	highLightEndPos = 0;
}

static bool _removeHighLightText(GuiTextAreaState& state, String& text)
{
	roSize& posBeg = state.highLightBegPos;
	roSize& posEnd = state.highLightEndPos;

	if(text.isEmpty()) {
		posBeg = posEnd = 0;
		return false;
	}

	roAssert(posEnd >= posBeg);
	roSize removeCount = roClampedSubtraction(posEnd, posBeg);
	text.erase(posBeg, removeCount);
	posEnd = posBeg;

	return removeCount > 0;
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

		roSize& posBeg = state.highLightBegPos;
		roSize& posEnd = state.highLightEndPos;

		// Handle text insertion
		const roUtf8* inputText = inputDriver->inputText(inputDriver);
		roSize inputTextLen = roStrLen(inputText);
		if(inputText && *inputText != '\0') {
			_removeHighLightText(state, text);
			roAssert(posBeg == posEnd);
			text.insert(posBeg, inputText, inputTextLen);
			posBeg = posEnd = posBeg + inputTextLen;
		}

		// Handle character removal
		if(!text.isEmpty())
		{
			if(inputDriver->buttonDown(inputDriver, stringHash("Back"), false)) {
				if(!_removeHighLightText(state, text)) {
					roAssert(posBeg == posEnd);
					if(posBeg > 0 && posBeg <= text.size()) {
						text.erase(posBeg - 1, 1);
						posBeg = posEnd = posBeg - 1;
					}
				}
			}

			if(inputDriver->buttonDown(inputDriver, stringHash("Delete"), false)) {
				if(!_removeHighLightText(state, text)) {
					roAssert(posBeg == posEnd);
					if(posBeg < text.size())
						text.erase(posBeg, 1);
				}
			}
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

		c.setFont("12pt \"DFKai-SB\"");
		c.setTextAlign("left");
		c.setTextBaseline("top");
		c.setGlobalColor(sStyle.textColor.data);
		c.fillText(text.c_str(), padding, padding, -1);
	guiEndScrollPanel();
}
