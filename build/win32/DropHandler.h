#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <oleidl.h>

class DropHandler
{
public:
	DropHandler(HWND hWnd);
	virtual ~DropHandler(void);
	// Drop event handlers
	virtual HRESULT DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) = 0;
	virtual HRESULT DragOver(DWORD rfKeyState, POINTL pt, DWORD* pdwEffect) = 0;
	virtual HRESULT DragLeave(void) = 0;
	virtual HRESULT Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) = 0;

private:
	class DropTarget : public IDropTarget
	{
	public:
		DropTarget(DropHandler& dropHandler);
		virtual ~DropTarget(void);

		// IUnknown implementation
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject);
		virtual ULONG   STDMETHODCALLTYPE AddRef(void);
		virtual ULONG   STDMETHODCALLTYPE Release(void);

		// IDropTarget implementation
		virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
		virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD rfKeyState, POINTL pt, DWORD* pdwEffect);
		virtual HRESULT STDMETHODCALLTYPE DragLeave(void);
		virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
	private:
		LONG m_refCount;
		DropHandler& m_dropHandler;
	};

	HWND       m_hWnd;
	DropTarget m_dropTarget;
};

struct Rhinoca;

class FileDropHandler : public DropHandler
{
public:
	typedef void (*OnDropCallback)(const char* filePath, void* userData);

	FileDropHandler(HWND hWnd, OnDropCallback callback, void* callbackUserData);
	// Drop event handlers
	virtual HRESULT DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
	virtual HRESULT DragOver(DWORD rfKeyState, POINTL pt, DWORD* pdwEffect);
	virtual HRESULT DragLeave(void);
	virtual HRESULT Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

private:
	bool GetFirstFilePathFromDataObject(IDataObject* dataObject, char* filePath, size_t maxPathLength);

	// We have to cache the dwEffect returned from DragEnter to get a correct drag cursor.
	DWORD m_lastEffect;

	OnDropCallback m_callback;
	void* m_callbackUserData;
};
