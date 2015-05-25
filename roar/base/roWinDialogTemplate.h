#ifndef __roWinDialogTemplate_h__
#define __roWinDialogTemplate_h__

#include "../platform/roCompiler.h"

#if roOS_WIN

#include "../platform/roPlatformHeaders.h"

namespace ro {

// Create window's dialog programmatically
// Reference:
// http://www.flipcode.com/archives/Dialog_Template.shtml
// http://www.codeproject.com/Articles/13330/Using-Dialog-Templates-to-create-an-InputBox-in-C
class WinDialogTemplate
{
public:
	WinDialogTemplate(
		roUtf8* caption, unsigned long style,
		int x, int y, int w, int h,
		roUtf8* font = NULL, unsigned short fontSize = 8);

	~WinDialogTemplate();

	void AddComponent(roUtf8* type, roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	void AddButton(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	void AddEditBox(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	void AddStatic(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	void AddListBox(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);
	
	void AddScrollBar(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	void AddComboBox(roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	// Returns a pointer to the Win32 dialog template which the object
	// represents. This pointer may become invalid if additional
	// components are added to the template.
	operator const DLGTEMPLATE*() const
	{
		return dialogTemplate;
	}

protected:
	void AlignData(int size);
	void AppendString(roUtf8* string);
	void AppendData(void* data, int dataLength);
	void EnsureSpace(int length);

	void AddStandardComponent(unsigned short type, roUtf8* caption, unsigned long style, unsigned long exStyle,
		int x, int y, int w, int h, unsigned short id);

	DLGTEMPLATE* dialogTemplate;
	int totalBufferLength;
	int usedBufferLength;
};	// WinDialogTemplate

}	// namespace ro

#endif

#endif	// __roWinDialogTemplate_h__
