#include "pch.h"
#include "roWinDialogTemplate.h"

#if roOS_WIN

namespace ro {

WinDialogTemplate::WinDialogTemplate(
	roUtf8* caption, DWORD style,
	int x, int y, int w, int h,
	roUtf8* font, WORD fontSize)
{
	usedBufferLength = sizeof(DLGTEMPLATE );
	totalBufferLength = usedBufferLength;

	dialogTemplate = (DLGTEMPLATE*)malloc(totalBufferLength);

	dialogTemplate->style = style;
		
	if(font != NULL)
		dialogTemplate->style |= DS_SETFONT;
		
	dialogTemplate->x     = (short)x;
	dialogTemplate->y     = (short)y;
	dialogTemplate->cx    = (short)w;
	dialogTemplate->cy    = (short)h;
	dialogTemplate->cdit  = 0;
		
	dialogTemplate->dwExtendedStyle = 0;

	// The dialog box doesn't have a menu or a special class
	AppendData("\0", 2);
	AppendData("\0", 2);

	// Add the dialog's caption to the template
	AppendString(caption);

	if (font != NULL)
	{
		AppendData(&fontSize, sizeof(WORD));
		AppendString(font);
	}	
}

WinDialogTemplate::~WinDialogTemplate()
{
	free(dialogTemplate);
}

void WinDialogTemplate::AddComponent(roUtf8* type, roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	DLGITEMTEMPLATE item;

	item.style = style;
	item.x     = (short)x;
	item.y     = (short)y;
	item.cx    = (short)w;
	item.cy    = (short)h;
	item.id    = id;

	item.dwExtendedStyle = exStyle;

	AppendData(&item, sizeof(DLGITEMTEMPLATE));
		
	AppendString(type);
	AppendString(caption);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));

	// Increment the component count
	dialogTemplate->cdit++;
}

void WinDialogTemplate::AddButton(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0080, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void WinDialogTemplate::AddEditBox(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0081, caption, style, exStyle, x, y, w, h, id);
	
	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void WinDialogTemplate::AddStatic(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0082, caption, style, exStyle, x, y, w, h, id);
	
	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void WinDialogTemplate::AddListBox(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0083, caption, style, exStyle, x, y, w, h, id);
	
	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}
	
void WinDialogTemplate::AddScrollBar(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0084, caption, style, exStyle, x, y, w, h, id);
	
	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void WinDialogTemplate::AddComboBox(roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	AddStandardComponent(0x0085, caption, style, exStyle, x, y, w, h, id);
	
	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void WinDialogTemplate::AddStandardComponent(WORD type, roUtf8* caption, DWORD style, DWORD exStyle,
	int x, int y, int w, int h, WORD id)
{
	DLGITEMTEMPLATE item;

	// DWORD algin the beginning of the component data
	AlignData(sizeof(DWORD));

	item.style = style;
	item.x     = (short)x;
	item.y     = (short)y;
	item.cx    = (short)w;
	item.cy    = (short)h;
	item.id    = id;

	item.dwExtendedStyle = exStyle;

	AppendData(&item, sizeof(DLGITEMTEMPLATE));
		
	WORD preType = 0xFFFF;
		
	AppendData(&preType, sizeof(WORD));
	AppendData(&type, sizeof(WORD));

	AppendString(caption);

	// Increment the component count
	dialogTemplate->cdit++;	
}

void WinDialogTemplate::AlignData(int size)
{
	int paddingSize = usedBufferLength % size;
		
	if (paddingSize != 0)
	{
		EnsureSpace(paddingSize);
		usedBufferLength += paddingSize;
	}
}

void WinDialogTemplate::AppendString(roUtf8* string)
{
	int length = MultiByteToWideChar(CP_ACP, 0, string, -1, NULL, 0);

	WCHAR* wideString = (WCHAR*)malloc(sizeof(WCHAR) * length);
	MultiByteToWideChar(CP_ACP, 0, string, -1, wideString, length);

	AppendData(wideString, length * sizeof(WCHAR));
	free(wideString);
}

void WinDialogTemplate::AppendData(void* data, int dataLength)
{
	EnsureSpace(dataLength);

	memcpy((char*)dialogTemplate + usedBufferLength, data, dataLength);
	usedBufferLength += dataLength;
}

void WinDialogTemplate::EnsureSpace(int length)
{
	if (length + usedBufferLength > totalBufferLength)
	{
		totalBufferLength += length * 2;

		void* newBuffer = malloc(totalBufferLength);
		memcpy(newBuffer, dialogTemplate, usedBufferLength);
			
		free(dialogTemplate);
		dialogTemplate = (DLGTEMPLATE*)newBuffer;
	}
}

}	// namespace ro

#endif
