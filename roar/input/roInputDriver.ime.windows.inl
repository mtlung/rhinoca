#include "../base/roArray.h"
#include "../base/roTypeCast.h"

#include <Textstor.h>
#include <msctf.h>
#include <msaatext.h>
#include <olectl.h>

// This file use the Text Service Framework on Windows to get get from user input
// Reference:
// Text Service Framework aware application: http://msdn.microsoft.com/en-us/ms538049
// http://hp.vector.co.jp/authors/VA050396/tech_01.html
// http://blogs.msdn.com/b/tsfaware/archive/2007/04/27/tsf-application-interfaces.aspx

class TextService : public ITextStoreACP, public ITfContextOwnerCompositionSink
{
public:
	TextService();
	~TextService();
 
	roStatus init();
	void close();

	void popText(ro::String& output);

	STDMETHOD_(DWORD, AddRef());
	STDMETHOD_(DWORD, Release()); 
	STDMETHOD(QueryInterface)(REFIID, LPVOID*);

	// ITextStoreACP interfaces
	STDMETHODIMP AdviseSink(REFIID, IUnknown*, DWORD);
	STDMETHODIMP UnadviseSink(IUnknown*);
	STDMETHODIMP RequestLock(DWORD, HRESULT*);
	STDMETHODIMP GetStatus(TS_STATUS*);
	STDMETHODIMP QueryInsert(LONG, LONG, ULONG, LONG*, LONG*);
	STDMETHODIMP GetSelection(ULONG, ULONG, TS_SELECTION_ACP*, ULONG*);
	STDMETHODIMP SetSelection(ULONG, const TS_SELECTION_ACP*);
	STDMETHODIMP GetText(LONG, LONG, WCHAR*, ULONG, ULONG*, TS_RUNINFO*, ULONG, ULONG*, LONG*);
	STDMETHODIMP SetText(DWORD, LONG, LONG, const WCHAR*, ULONG, TS_TEXTCHANGE*);
	STDMETHODIMP InsertTextAtSelection(DWORD, const WCHAR*, ULONG, LONG*, LONG*, TS_TEXTCHANGE*);

	STDMETHODIMP GetEndACP(LONG*);
	STDMETHODIMP GetActiveView(TsViewCookie*);
	STDMETHODIMP GetACPFromPoint(TsViewCookie, const POINT*, DWORD, LONG*) { return E_NOTIMPL; }
	STDMETHODIMP GetTextExt(TsViewCookie, LONG, LONG, RECT*, BOOL*);
	STDMETHODIMP GetScreenExt(TsViewCookie, RECT*);
	STDMETHODIMP GetWnd(TsViewCookie, HWND*);

	STDMETHODIMP GetFormattedText(LONG, LONG, IDataObject**) { return E_NOTIMPL; }
	STDMETHODIMP GetEmbedded(LONG, REFGUID, REFIID, IUnknown**) { return E_NOTIMPL; }
	STDMETHODIMP QueryInsertEmbedded(const GUID*, const FORMATETC*, BOOL*) { return E_NOTIMPL; }
	STDMETHODIMP InsertEmbedded(DWORD, LONG, LONG, IDataObject*, TS_TEXTCHANGE*) { return E_NOTIMPL; }
	STDMETHODIMP InsertEmbeddedAtSelection(DWORD, IDataObject*, LONG*, LONG*, TS_TEXTCHANGE*) { return E_NOTIMPL; }
	STDMETHODIMP RequestSupportedAttrs(DWORD, ULONG, const TS_ATTRID*) { return E_NOTIMPL; }
	STDMETHODIMP RequestAttrsAtPosition(LONG, ULONG, const TS_ATTRID*, DWORD) { return E_NOTIMPL; }
	STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG, ULONG, const TS_ATTRID*, DWORD) { return E_NOTIMPL; }
	STDMETHODIMP FindNextAttrTransition(LONG, LONG, ULONG, const TS_ATTRID*, DWORD, LONG*, BOOL*, LONG*) { return E_NOTIMPL; }
	STDMETHODIMP RetrieveRequestedAttrs(ULONG, TS_ATTRVAL*, ULONG*) { return E_NOTIMPL; }

	// ITfContextOwnerCompositionSink interfaces
	STDMETHODIMP OnStartComposition(ITfCompositionView* pComposition, BOOL* pfOk);
	STDMETHODIMP OnUpdateComposition(ITfCompositionView* pComposition, ITfRange* pRangeNew);
	STDMETHODIMP OnEndComposition(ITfCompositionView* pComposition);

// Private
	ULONG refCount;
	CComPtr<ITextStoreACPSink> sink;
	ITfCompositionView* currentCompositionView;
 
	LONG _acpStart;
	LONG _acpEnd;

	HWND _hWnd;
	ro::Array<wchar_t> _wString;
	ro::String _outputString;

	CComPtr<ITfThreadMgr> _cpThreadMgr;
	CComPtr<ITfDocumentMgr> _cpDocMgr;
	CComPtr<ITfContext> _cpContext;
};

TextService::TextService()
{
	_hWnd = 0;
	refCount = 0;
	sink = NULL;
	currentCompositionView = NULL;
	_acpStart = _acpEnd = 0;
}

TextService::~TextService()
{
	roAssert(!_cpThreadMgr && !_cpDocMgr && "call close() before destory");
}

roStatus TextService::init()
{
	roStatus st;

	// Initialize COM
	// NOTE: Must use COINIT_MULTITHREADED otherwise _cpThreadMgr->Activate() will fail under window's fiber
	HRESULT hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if(FAILED(hr)) return st;

	_hWnd = ::GetActiveWindow();

	// Register with TSF...
	hr = ::CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, (void**)&_cpThreadMgr);
	if(FAILED(hr) || !_cpThreadMgr) return st;

	TfClientId clientId;
	if(FAILED(_cpThreadMgr->Activate(&clientId))) return st;

	if(FAILED(_cpThreadMgr->CreateDocumentMgr(&_cpDocMgr)) || !_cpDocMgr) return st;

	TfEditCookie ecTextStore;
	if(FAILED(_cpDocMgr->CreateContext(clientId, 0, (ITextStoreACP*)this, &_cpContext, &ecTextStore)) || !_cpContext) return st;

	if(FAILED(_cpDocMgr->Push(_cpContext))) return st;

	ITfDocumentMgr* pDocumentMgrPrev = NULL;
	_cpThreadMgr->AssociateFocus(_hWnd, _cpDocMgr, &pDocumentMgrPrev);
	if(pDocumentMgrPrev)
		pDocumentMgrPrev->Release();

	AddRef();

	return roStatus::ok;
}

void TextService::close()
{
	if(!_cpThreadMgr || !_cpDocMgr)
		return;

	_cpThreadMgr = NULL;
	_cpDocMgr = NULL;

	::CoUninitialize();

	Release();
}

void TextService::popText(ro::String& output)
{
	output = _outputString;
	_outputString.clear();
}

STDMETHODIMP_(DWORD) TextService::AddRef()
{
	return ++refCount;
}

STDMETHODIMP_(DWORD) TextService::Release()
{
	DWORD retval = --refCount;
	if(retval == 0)
		delete this;
	return retval;
}

STDMETHODIMP TextService::QueryInterface(REFIID riid, LPVOID* pVoid)
{
	*pVoid = NULL;

	if(IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITextStoreACP))
		*pVoid = (ITextStoreACP*)this;
	else if(IsEqualIID(riid, IID_ITfContextOwnerCompositionSink))
		*pVoid = (ITfContextOwnerCompositionSink*)this;

	if(*pVoid)
		return S_OK;

	return E_NOINTERFACE;
}

STDMETHODIMP TextService::AdviseSink(REFIID riid, IUnknown* punk, DWORD dwMask)
{
	if(!IsEqualGUID(riid, IID_ITextStoreACPSink))
		return TS_E_NOOBJECT;

	if(FAILED(punk->QueryInterface(IID_ITextStoreACPSink, (void**)&sink)))
		return E_NOINTERFACE;

	return S_OK;
}

STDMETHODIMP TextService::UnadviseSink(IUnknown* punk)
{
	if(sink && sink == punk) {
		sink = NULL;
		return S_OK;
	}
	else
		return CONNECT_E_NOCONNECTION;
}

STDMETHODIMP TextService::RequestLock(DWORD dwLockFlags, HRESULT* phrSession)
{
	if(!sink) return E_UNEXPECTED;

	*phrSession = sink->OnLockGranted(dwLockFlags);
	return S_OK;
}

STDMETHODIMP TextService::GetStatus(TS_STATUS* pdcs)
{
	pdcs->dwDynamicFlags = 0;
	pdcs->dwStaticFlags = 0;
	return S_OK;
}

STDMETHODIMP TextService::QueryInsert(LONG acpTestStart, LONG acpTestEnd, ULONG cch, LONG* pacpResultStart, LONG* pacpResultEnd)
{
	sink->OnLayoutChange(TS_LC_CHANGE, 0x01);
	*pacpResultStart = acpTestStart;
	*pacpResultEnd = acpTestEnd;
	return S_OK;
}

STDMETHODIMP TextService::GetSelection(ULONG ulIndex, ULONG ulCount, TS_SELECTION_ACP* pSelection, ULONG* pcFetched)
{
	*pcFetched = 0;
	if((ulCount > 0) && ((ulIndex == 0) || (ulIndex == TS_DEFAULT_SELECTION)))
	{
		pSelection[0].acpStart = _acpStart;
		pSelection[0].acpEnd = _acpEnd;
		pSelection[0].style.ase = TS_AE_END;
		pSelection[0].style.fInterimChar = FALSE;
		*pcFetched = 1;
	}

	return S_OK;
}

STDMETHODIMP TextService::SetSelection(ULONG ulCount, const TS_SELECTION_ACP* pSelection)
{
	if(ulCount > 0) {
		_acpStart = pSelection[0].acpStart;
		_acpEnd = pSelection[0].acpEnd;
	}

	return S_OK;
}

STDMETHODIMP TextService::GetText(LONG acpStart, LONG acpEnd, WCHAR* pchPlain, ULONG cchPlainReq, ULONG* pcchPlainRet, TS_RUNINFO* prgRunInfo, ULONG cRunInfoReq, ULONG* pcRunInfoRet, LONG* pacpNext)
{
	if(cchPlainReq == 0 && cRunInfoReq == 0)
		return S_OK;

	if(acpEnd == -1)
		acpEnd = _acpEnd;

	acpEnd = roMinOf3(num_cast<ULONG>(acpEnd), acpStart + cchPlainReq, num_cast<ULONG>(_wString.size()));

	memcpy(pchPlain, _wString.typedPtr(), (acpEnd - acpStart) * sizeof(wchar_t));

	*pcchPlainRet = acpEnd - acpStart;
	if(cRunInfoReq)
	{
		prgRunInfo[0].uCount = acpEnd - acpStart;
		prgRunInfo[0].type = TS_RT_PLAIN;
		*pcRunInfoRet = 1;
	}

	*pacpNext = acpEnd;

	return S_OK;
}

STDMETHODIMP TextService::SetText(DWORD dwFlags, LONG acpStart, LONG acpEnd, const WCHAR* pchText, ULONG cch, TS_TEXTCHANGE* pChange)
{
	if(num_cast<size_t>(acpStart) > _wString.size())
		return E_INVALIDARG;

	roSize acpRemovingEnd = roMinOf2(num_cast<roSize>(acpEnd), _wString.size() + 1);

	for(roSize i=0; i<acpRemovingEnd - acpStart; ++i)
		_wString.removeAt(acpStart);

	_wString.insert(acpStart, pchText, cch);

	pChange->acpStart = acpStart;
	pChange->acpOldEnd = acpEnd;
	pChange->acpNewEnd = acpStart + cch;

	return S_OK;
}

STDMETHODIMP TextService::InsertTextAtSelection(DWORD dwFlags, const WCHAR* pchText, ULONG cch, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange)
{
	if (dwFlags & TS_IAS_QUERYONLY) {
		*pacpStart = _acpStart;
		*pacpEnd = _acpStart + cch;
		return S_OK;
	}

	for(roSize i=0; i<num_cast<roSize>(_acpEnd - _acpStart); ++i)
		_wString.removeAt(_acpStart);

	_wString.insert(_acpStart, pchText, cch);

	if(pacpStart)
		*pacpStart = _acpStart;

	if(pacpEnd)
		*pacpEnd = _acpStart + cch;

	if(pChange) {
		pChange->acpStart = _acpStart;
		pChange->acpOldEnd = _acpEnd;
		pChange->acpNewEnd = _acpStart + cch;
	}

	// Set new end
	_acpEnd = _acpStart + cch;

	return S_OK;
}

STDMETHODIMP TextService::GetEndACP(LONG* pacp)
{
	*pacp = _acpEnd;
	return S_OK;
}

STDMETHODIMP TextService::GetActiveView(TsViewCookie* pvcView)
{
	*pvcView = 0;
	return S_OK;
}

STDMETHODIMP TextService::GetTextExt(TsViewCookie, LONG, LONG, RECT* prc, BOOL* pfClipped)
{
	prc->left = 0;
	prc->right = 0;
	prc->top = 0;
	prc->bottom = 0;

	::ClientToScreen(_hWnd, (POINT *)&prc->left);
	::ClientToScreen(_hWnd, (POINT *)&prc->right);
	*pfClipped = FALSE;

	return S_OK;
}

STDMETHODIMP TextService::GetScreenExt(TsViewCookie vcView, RECT* prc)
{
	::GetClientRect(_hWnd, prc);
	::ClientToScreen(_hWnd, (POINT*)&prc->left);
	::ClientToScreen(_hWnd, (POINT*)&prc->right);
	return S_OK;
}

STDMETHODIMP TextService::GetWnd(TsViewCookie vcView, HWND* phwnd)
{
	*phwnd = _hWnd;
	return S_OK;
}

STDMETHODIMP TextService::OnStartComposition(ITfCompositionView* pComposition, BOOL* pfOk)
{
	roAssert(_wString.isEmpty());
	return S_OK;
}

STDMETHODIMP TextService::OnUpdateComposition(ITfCompositionView* pComposition, ITfRange* pRangeNew)
{
	return S_OK;
}

STDMETHODIMP TextService::OnEndComposition(ITfCompositionView* pComposition)
{
	ro::String tmp;
	roVerify(tmp.fromUtf16(_wString.castedPtr<roUint16>(), _wString.size()));
	_outputString += tmp;
	_wString.clear();
	_acpStart = _acpEnd = 0;

	return S_OK;
}
