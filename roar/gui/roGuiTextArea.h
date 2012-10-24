GuiTextAreaState::GuiTextAreaState()
{
	highLightBegPos = 0;
	highLightEndPos = 0;
}

static bool _removeHighLightText(GuiTextAreaState& state, String& text, roSize& posBeg, roSize& posEnd)
{
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

		lineIndice.clear();
		if(len)
			lineIndice.pushBack(0);

		for(roUint16 w; len;)
		{
			int utf8Consumed = roUtf8ToUtf16Char(w, str, len);
			if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

			str += utf8Consumed;
			len -= utf8Consumed;

			if(w == L'\n')
				lineIndice.pushBack(str - text.c_str());
		}
	}

	roSize incrementCharIdx(const String& str, roSize idx)
	{
		roUint16 w;
		if(str.isEmpty())
			return 0;

		if(!str.isInRange(idx))
			return idx;

		return idx + roUtf8ToUtf16Char(w, &str[idx], roClampedSubtraction(str.size(), idx));
	}

	roSize decrementCharIdx(const String& str, roSize idx)
	{
		if(str.isEmpty())
			return 0;

		// Snap back to leading character
		do {
			idx = roClampedSubtraction(idx, roSize(1u));
		} while(idx > 0 && (str[idx] & 0xC0) == 0x80);

		return idx;
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

		if(!lineIndice.isInRange(lineIdx))
			return 0;

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

	Vec2 getLineEndPos(const roUtf8* str, roSize lineIdx, Canvas& c)
	{
		if(!lineIndice.isInRange(lineIdx))
			return Vec2(0.f);

		roSize lineBegCharIdx = lineIndice[lineIdx];
		roSize charIdx = lineBegCharIdx + getLineLength(lineIdx);

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
		_removeHighLightText(state, text, posBeg, posEnd);
		roAssert(posBeg == posEnd);
		text.insert(posBeg, inputText, inputTextLen);
		posBeg = posEnd = posBeg + inputTextLen;
	}

	// Handle character removal
	if(!text.isEmpty())
	{
		if(inputDriver->buttonDown(inputDriver, stringHash("Back"), false)) {
			if(!_removeHighLightText(state, text, posBeg, posEnd)) {
				roAssert(posBeg == posEnd);
				if(posBeg > 0 && posBeg <= text.size()) {
					text.erase(posBeg - 1, 1);
					posBeg = posEnd = posBeg - 1;
				}
			}
		}

		if(inputDriver->buttonDown(inputDriver, stringHash("Delete"), false)) {
			if(!_removeHighLightText(state, text, posBeg, posEnd)) {
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
			posEnd = layout.decrementCharIdx(text, posEnd);
			if(!shift) posBeg = posEnd;
		} else

		if(inputDriver->buttonDown(inputDriver, stringHash("Right"), false)) {
			posEnd = layout.incrementCharIdx(text, posEnd);
			if(!shift) posBeg = posEnd;
		} else

		if(inputDriver->buttonDown(inputDriver, stringHash("Up"), false)) {
			posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y - lineSpacing - 1, c);
			if(!shift) posBeg = posEnd;
		} else

		if(inputDriver->buttonDown(inputDriver, stringHash("Down"), false)) {
			posEnd = layout.getCharIdxFromPos(text.c_str(), coordEnd.x, coordEnd.y + 1, c);
			if(!shift) posBeg = posEnd;
		} else

		if(inputDriver->buttonDown(inputDriver, stringHash("Home"), false)) {
			posEnd = layout.getLineBegCharIdx(posEnd);
			if(!shift) posBeg = posEnd;
		} else

		if(inputDriver->buttonDown(inputDriver, stringHash("End"), false)) {
			posEnd = layout.getLineEndCharIdx(posEnd);
			if(!shift) posBeg = posEnd;
		}

		// Make sure the range is correct
		posBeg = roClamp(posBeg, roSize(0u), text.size());
		posEnd = roClamp(posEnd, roSize(0u), text.size());
	}

	{	// Handle mouse inputs
		bool shift = (inputDriver->button(inputDriver, stringHash("Shift"), false));

		if(_states.mouseDown()) {
			float x = _states.mousex() - padding;
			float y = _states.mousey() - padding;
			posEnd = layout.getCharIdxFromPos(text.c_str(), x, y, c);
			if(!shift) posBeg = posEnd;
		}

		if(_states.hotObject == &state) {
			float x = _states.mousex() - padding;
			float y = _states.mousey() - padding;
			posEnd = layout.getCharIdxFromPos(text.c_str(), x, y, c);
		}
	}

	// Update the pixel positions
	coordBeg = layout.getPosFromCharIdx(text.c_str(), posBeg, c);
	coordEnd = (posEnd == posBeg) ? coordBeg : layout.getPosFromCharIdx(text.c_str(), posEnd, c);

	// Make sure the caret can be seen on screen
	if(posBeg != state.highLightBegPos || posEnd != state.highLightEndPos) {
		float diff = 0;
		float torrence = lineSpacing * 3;

		// Horizontal
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

		// Vertical
		if((diff = state.vScrollBar.value - coordEnd.y) > -lineSpacing) {
			if(diff > torrence)
				state.vScrollBar.value = coordEnd.y - torrence;
			state.vScrollBar.value -= torrence;
		}
		if((diff = coordEnd.y - state.vScrollBar.value - state._clientRect.bottom()) > 0) {
			if(diff > torrence)
				state.vScrollBar.value = coordEnd.y;
			state.vScrollBar.value += torrence;
		}
	}

	state.highLightBegPos = posBeg;
	state.highLightEndPos = posEnd;

	{	// Draw highlight
		// It's possible the beg is larger than end, in this case swap them.
		roSize beg = posBeg;
		roSize end = posEnd;
		float coordBegX = coordBeg.x;
		float coordEndX = coordEnd.x;
		float coordBegY = coordBeg.y;
		if(end < beg) {
			roSwap(beg, end);
			roSwap(coordBegX, coordEndX);
			coordBegY = coordEnd.y;
		}

		roSize begLine = layout.getLineIdxForCharIdx(beg);
		roSize endLine = layout.getLineIdxForCharIdx(end);

		// Draw hight light line by line
		c.setFillColor(0, 0, 0, 1);
		for(roSize i=begLine; i<=endLine; ++i) {
			float x1 = (i == begLine) ? coordBegX : 0;
			float x2 = (i == endLine) ? coordEndX : layout.getLineEndPos(text.c_str(), i, c).x;
			float y1 = coordBegY + (i - begLine) * lineSpacing - lineSpacing;
			float y2 = y1 + lineSpacing;

			c.fillRect(
				padding + x1,
				padding + y1,
				x2 - x1,
				y2 - y1
			);
		}
	}

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
