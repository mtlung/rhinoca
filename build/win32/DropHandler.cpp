#include <stdio.h>
#include <windows.h> // Load the complete set of win32api.

#include "DropHandler.h"

#pragma warning(push)
#pragma warning(disable : 4355) // Justification: DropTarget does not invoke any functions on DropHandler. It just stores DropTarget's pointer.
DropHandler::DropHandler(HWND hWnd) : m_hWnd(hWnd), m_dropTarget(*this)
#pragma warning(pop)
{
	if (RegisterDragDrop(m_hWnd, &m_dropTarget) != S_OK)
	{
		printf("RegisterDragDrop is failed.\n");
	}
}

DropHandler::~DropHandler(void)
{
	RevokeDragDrop(m_hWnd);
}

DropHandler::DropTarget::DropTarget(DropHandler& dropHandler) : m_refCount(1), m_dropHandler(dropHandler)
{
}

DropHandler::DropTarget::~DropTarget(void)
{
}

HRESULT DropHandler::DropTarget::QueryInterface(REFIID iid, void** ppvObject)
{
	if (iid == IID_IDropTarget || iid == IID_IUnknown)
	{
		AddRef();
		*ppvObject = this;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

ULONG   DropHandler::DropTarget::AddRef(void)
{
	return InterlockedIncrement(&m_refCount);
}

ULONG   DropHandler::DropTarget::Release(void)
{
	LONG count = InterlockedDecrement(&m_refCount);
		
	if (count == 0)
	{
		delete this;
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT DropHandler::DropTarget::DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return m_dropHandler.DragEnter(pDataObject, grfKeyState, pt, pdwEffect);
}

HRESULT DropHandler::DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return m_dropHandler.DragOver(grfKeyState, pt, pdwEffect);
}

HRESULT DropHandler::DropTarget::DragLeave(void)
{
	return m_dropHandler.DragLeave();
}

HRESULT DropHandler::DropTarget::Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return m_dropHandler.Drop(pDataObject, grfKeyState, pt, pdwEffect);
}

FileDropHandler::FileDropHandler(HWND hWnd, OnDropCallback callback, void* callbackUserData)
	: DropHandler(hWnd)
	, m_callback(callback)
	, m_callbackUserData(callbackUserData)
	, m_lastEffect(DROPEFFECT_NONE)
{
}

#define sizeof_static_array(x) sizeof(x) / sizeof(*x)

HRESULT FileDropHandler::DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	char filePath[MAX_PATH];

	if (!GetFirstFilePathFromDataObject(pDataObject, filePath, sizeof_static_array(filePath)))
		*pdwEffect = DROPEFFECT_NONE;
	else
		*pdwEffect = DROPEFFECT_MOVE;

	m_lastEffect = *pdwEffect;

	return S_OK;
}

HRESULT FileDropHandler::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	*pdwEffect = m_lastEffect;

	return S_OK;
}

HRESULT FileDropHandler::DragLeave(void)
{
	m_lastEffect = DROPEFFECT_NONE;

	return S_OK;
}

HRESULT FileDropHandler::Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	char filePath[MAX_PATH];

	if (!GetFirstFilePathFromDataObject(pDataObject, filePath, sizeof_static_array(filePath)))
		*pdwEffect = DROPEFFECT_NONE;
	else
	{
		*pdwEffect = DROPEFFECT_MOVE;
		m_callback(filePath, m_callbackUserData);
	}

	return S_OK;
}

bool FileDropHandler::GetFirstFilePathFromDataObject(IDataObject* dataObject, char* filePath, size_t maxPathLength)
{
	// Dropped files are stored in a HDROP.
	FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgm;

	bool ret = false;

	if (SUCCEEDED(dataObject->GetData(&fmte, &stgm)))
	{
		HDROP hdrop = reinterpret_cast<HDROP>(stgm.hGlobal);

		UINT cFiles = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);

		if (cFiles > 0)
		{
			// We load the first selected file only.
			UINT returnedFilePathLen = DragQueryFileA(hdrop, 0, filePath, maxPathLength);
			ret = returnedFilePathLen > 0 && returnedFilePathLen < maxPathLength;
		}
		ReleaseStgMedium(&stgm);
	}

	return ret;
}
