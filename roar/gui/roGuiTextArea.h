GuiTextAreaState::GuiTextAreaState()
{
	highLightBegPos = 0;
	highLightEndPos = 0;
}

static bool _removeHighLightText(GuiTextAreaState& state, String& text)
{
	roSize& posBeg = state.highLightBegPos;
	roSize& posEnd = state.highLightEndPos;

	// It's possible the beg is larger than end, in this case swap them.
	roSize beg = posBeg;
	roSize end = posEnd;
	if(end < beg)
		roSwap(beg, end);

	if(text.isEmpty()) {
		posBeg = posEnd = 0;
		return false;
	}

	roAssert(end >= beg);
	roSize removeCount = roClampedSubtraction(end, beg);
	text.erase(beg, removeCount);
	posEnd = posBeg = beg;

	return removeCount > 0;
}

namespace {

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

	Vec2 getPosFromCharIdx(const roUtf8* str, roSize charIdx, Canvas& c)
	{
		roSize lineIdx = getLineIdxForCharIdx(charIdx);
		roSize lineBegCharIdx = getLineBegCharIdx(charIdx);
		roAssert(charIdx >= lineBegCharIdx);

		TextMetrics metrics;
		metrics.width = 0;
		metrics.height = lineIdx * c.lineSpacing();
		c.measureText(str + lineBegCharIdx, charIdx - lineBegCharIdx, FLT_MAX, metrics);

		return Vec2(metrics.width, metrics.height);
	}

	roSize utfLen;
	Array<roSize> lineIndice;	// Map line beginning to char index
};	// Layout

}	// namespace

void guiTextArea(GuiTextAreaState& state, String& text)
{
	state.deducedRect = state.rect;
	_setContentExtend(state, guiSkin.textArea, Sizef());
	_updateWigetState(state);

	Canvas& c = *_states.canvas;
	c.setFont("16pt \"DFKai-SB\"");

	roSize posBeg = state.highLightBegPos;
	roSize posEnd = state.highLightEndPos;

	// Pre-process a map to the first line character
	Layout layout;
	layout.init(text);

guiBeginScrollPanel(state);

	GuiWigetState labelState;
	Sizef textExtend = _calTextExtend(text.c_str());
	_setContentExtend(labelState, guiSkin.textArea, textExtend);
	_updateWigetState(labelState);

	float padding = guiSkin.textArea.padding;
	float lineSpacing = c.lineSpacing();
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
	Vec2 coordBeg = layout.getPosFromCharIdx(text.c_str(), posBeg, c);
	Vec2 coordEnd = (posEnd == posBeg) ? coordBeg : layout.getPosFromCharIdx(text.c_str(), posEnd, c);;

	{	// Handle keyboard input to manipulate the caret position
		bool shift = (inputDriver->button(inputDriver, stringHash("Shift"), false));

		if(inputDriver->buttonDown(inputDriver, stringHash("Left"), false)) {
			posEnd = roClampedSubtraction(posEnd, 1u);
			if(!shift) posBeg = posEnd;
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("Right"), false)) {
			posEnd = posEnd + 1;
			if(!shift) posBeg = posEnd;
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("Up"), false)) {
			posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y - lineSpacing - 1, c);
			if(!shift) posBeg = posEnd;
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("Down"), false)) {
			posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y + 1, c);
			if(!shift) posBeg = posEnd;
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("Home"), false)) {
			posEnd = layout.getLineBegCharIdx(posBeg);
			if(!shift) posBeg = posEnd;
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("End"), false)) {
			posEnd = layout.getLineEndCharIdx(posEnd);
			if(!shift) posBeg = posEnd;
		}

		// Make sure the range is correct
		posBeg = roClamp(posBeg, 0u, text.size());
		posEnd = roClamp(posEnd, 0u, text.size());
	}

	// Update the pixel positions
	coordBeg = layout.getPosFromCharIdx(text.c_str(), posBeg, c);
	coordEnd = (posEnd == posBeg) ? coordBeg : layout.getPosFromCharIdx(text.c_str(), posEnd, c);

	// Make sure the caret can be seen on screen
	if(posBeg != state.highLightBegPos || posEnd != state.highLightEndPos) {
		float diff = 0;
		float torrence = lineSpacing * 3;
		if((diff = state.hScrollBar.value - coordEnd.x) > 0) {
			if(diff > torrence)
				state.hScrollBar.value = coordEnd.x - torrence;
			state.hScrollBar.value -= torrence;
		}
		if((diff = coordEnd.x - state.hScrollBar.value - state._clientRect.right()) > 0) {
			if(diff > torrence)
				state.hScrollBar.value = coordEnd.x;
			state.hScrollBar.value += torrence;
		}
	}

	state.highLightBegPos = posBeg;
	state.highLightEndPos = posEnd;

	// Draw highlight
	c.setFillColor(0, 0, 0, 1);
	c.fillRect(padding + coordBeg.x, padding + coordBeg.y - lineSpacing, coordEnd.x - coordBeg.x, coordEnd.y - coordBeg.y + lineSpacing);

	// Draw text
	c.setTextAlign("left");
	c.setTextBaseline("top");
	c.setGlobalColor(sStyle.textColor.data);
	c.fillText(text.c_str(), padding, padding, -1);

	// Draw caret
	c.beginPath();
	c.moveTo(padding + coordEnd.x, padding + coordEnd.y);
	c.lineTo(padding + coordEnd.x, padding + coordEnd.y - lineSpacing);
	c.stroke();

guiEndScrollPanel();
}
