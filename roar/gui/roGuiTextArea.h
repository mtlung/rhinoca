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

struct Layout
{
	void init(const String& text)
	{
		const roUtf8* str = text.c_str();
		utfLen = text.size();
		roSize len = utfLen;
		roSize wlen = 0;

		lineIndice.clear();
		if(len)
			lineIndice.pushBack(0);

		for(roUint16 w; len;)
		{
			int utf8Consumed = roUtf8ToUtf16Char(w, str, len);
			if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

			str += utf8Consumed;
			len -= utf8Consumed;
			wlen++;

			if(w == L'\n')
				lineIndice.pushBack(wlen);
		}
	}

	roSize getLineLength(roSize lineIdx)
	{
		if(!lineIndice.isInRange(lineIdx))
			return 0;
		if(!lineIndice.isInRange(lineIdx + 1))
			return utfLen - lineIndice[lineIdx];
		else
			return lineIndice[lineIdx + 1] - lineIndice[lineIdx] - 1;	// -1 for excluding '\n'
	}

	roSize getLineIdxForCharIdx(roSize charIdx)
	{
		roSize* i = roUpperBound(lineIndice.begin(), lineIndice.size(), charIdx);
		if(i)
			return i - lineIndice.begin() - 1;
		else
			return lineIndice.isEmpty() ? 0 : lineIndice.size() - 1;
	}

	roSize getLineBegCharIdx(roSize charIdx)
	{
		roSize currentLine = getLineIdxForCharIdx(charIdx);
		if(!lineIndice.isInRange(currentLine))
			return 0;
		return lineIndice[currentLine];
	}

	roSize getLineEndCharIdx(roSize charIdx)
	{
		roSize currentLine = getLineIdxForCharIdx(charIdx);
		if(!lineIndice.isInRange(currentLine + 1))
			return utfLen;
		return lineIndice[currentLine + 1] - 1;
	}

	roSize getLineIdxFromPos(float x, float y, Canvas& c)
	{
		float lineSpacing = c.lineSpacing();
		if(lineSpacing == 0 || lineIndice.isEmpty())
			return 0;

		return roMinOf2(lineIndice.size() - 1, roSize(y / lineSpacing));
	}

	roSize getCharIdxFromPos(const roUtf8* str, float x, float y, Canvas& c)
	{
		roSize lineIdx = getLineIdxFromPos(x, y, c);
		roSize lineLength = getLineLength(lineIdx);
		roSize firstCharIdx = lineIndice[lineIdx];
		roSize charIdx = firstCharIdx;

		const roUtf8* p = str + lineIndice[lineIdx];

		for(roSize i=0; i<=lineLength; ++i) {
			TextMetrics metrics;
			c.measureText(p, i, FLT_MAX, metrics);
			charIdx = firstCharIdx + i;
			if(metrics.width >= x)
				break;
		}

		return charIdx;
	}

	roSize utfLen;
	Array<roSize> lineIndice;	// Map line beginning to char index
};

void guiTextArea(GuiTextAreaState& state, String& text)
{
	state.deducedRect = state.rect;
	_setContentExtend(state, guiSkin.textArea, Sizef());
	_updateWigetState(state);

	Canvas& c = *_states.canvas;
	c.setFont("16pt \"DFKai-SB\"");

	roSize& posBeg = state.highLightBegPos;
	roSize& posEnd = state.highLightEndPos;

	// Pre-process a map to the first line character
	Layout layout;
	layout.init(text);

guiBeginScrollPanel(state);

	GuiWigetState labelState;
	Sizef textExtend = _calTextExtend(text.c_str());
	_setContentExtend(labelState, guiSkin.textArea, textExtend);
	_updateWigetState(labelState);

	float padding = guiSkin.textArea.padding;
	const GuiStyle::StateSensitiveStyle& sStyle = _selectStateSensitiveSytle(labelState, guiSkin.textArea);

	roInputDriver* inputDriver = roSubSystems->inputDriver;

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

	// Get highlighting pixel positions
	Vec2 coordBeg, coordEnd;
	{
		roSize lineIdx = layout.getLineIdxForCharIdx(posBeg);
		roSize lineBegCharIdx = layout.getLineBegCharIdx(posBeg);
		roAssert(posBeg >= lineBegCharIdx);

		TextMetrics metrics;
		metrics.width = 0;
		metrics.height = lineIdx * c.lineSpacing();
		c.measureText(text.c_str() + lineBegCharIdx, posBeg - lineBegCharIdx, FLT_MAX, metrics);

		coordBeg.x = metrics.width;
		coordBeg.y = metrics.height;
	}

	if(posEnd != posBeg) {
		roSize lineIdx = layout.getLineIdxForCharIdx(posEnd);
		roSize lineBegCharIdx = layout.getLineBegCharIdx(posEnd);
		roAssert(posEnd >= lineBegCharIdx);

		TextMetrics metrics;
		metrics.width = 0;
		metrics.height = lineIdx * c.lineSpacing();
		c.measureText(text.c_str() + lineBegCharIdx, posEnd - lineBegCharIdx, FLT_MAX, metrics);

		coordEnd.x = metrics.width;
		coordEnd.y = metrics.height;
	}
	else
		coordEnd = coordBeg;

	{	// Handle keyboard input to manipulate the carte position
		if(inputDriver->buttonDown(inputDriver, stringHash("Left"), false))
			posBeg = posEnd = roClampedSubtraction(posBeg, 1u);

		if(inputDriver->buttonDown(inputDriver, stringHash("Right"), false))
			posBeg = posEnd = posEnd + 1;

		if(inputDriver->buttonDown(inputDriver, stringHash("Up"), false))
			posBeg = posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y - c.lineSpacing() - 1, c);

		if(inputDriver->buttonDown(inputDriver, stringHash("Down"), false))
			posBeg = posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y + 1, c);

		if(inputDriver->buttonDown(inputDriver, stringHash("Home"), false))
			posBeg = posEnd = layout.getLineBegCharIdx(posBeg);

		if(inputDriver->buttonDown(inputDriver, stringHash("End"), false))
			posBeg = posEnd = layout.getLineEndCharIdx(posEnd);

		// Make sure the range is correct
		posBeg = roClamp(posBeg, 0u, text.size());
		posEnd = roClamp(posEnd, 0u, text.size());
	}

	c.setTextAlign("left");
	c.setTextBaseline("top");
	c.setGlobalColor(sStyle.textColor.data);
	c.fillText(text.c_str(), padding, padding, -1);

	// Draw caret
	c.beginPath();
	c.moveTo(padding + coordEnd.x, padding + coordEnd.y);
	c.lineTo(padding + coordEnd.x, padding + coordEnd.y - c.lineSpacing());
	c.stroke();

guiEndScrollPanel();
}
