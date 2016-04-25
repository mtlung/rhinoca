#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/base/roResource.h"

GraphicsTestBase::GraphicsTestBase()
	: hWnd(0), driver(NULL), context(NULL)
{
}

GraphicsTestBase::~GraphicsTestBase()
{
	if(subSystems.resourceMgr)
		subSystems.resourceMgr->shutdown();

	if(driver) {
		driver->deleteShader(vShader);
		driver->deleteShader(gShader);
		driver->deleteShader(pShader);
	}

	if(hWnd) {
		::ShowWindow(hWnd, false);
		::PostQuitMessage(0);
		while(keepRun()) {}
		::DestroyWindow(hWnd);
	}

	if(HMODULE hModule = ::GetModuleHandle(NULL))
		::UnregisterClassW(windowClass, hModule);

	subSystems.shutdown();
}

static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GraphicsTestBase* test = reinterpret_cast<GraphicsTestBase*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if(uMsg == WM_CLOSE) PostQuitMessage(0);

	if(test) {
		roUserData eventData[4] = { (roUserData)hWnd, (roUserData)uMsg, (roUserData)wParam, lParam };
		test->subSystems.processEvents(eventData, roCountof(eventData));
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GraphicsTestBase::createWindow(int width, int height)
{
	width = width <= 0 ? CW_USEDEFAULT : width;
	height = height <= 0 ? CW_USEDEFAULT : height;

	hWnd = NULL;
	HMODULE hModule = ::GetModuleHandle(NULL);

	// Register window class
	WNDCLASSW wc;
	::ZeroMemory(&wc, sizeof(wc));
	// CS_OWNDC is an optimization, see page 659 of OpenGL SuperBible 4th edition
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.hInstance = hModule;
	wc.lpfnWndProc = &wndProc;
	wc.lpszClassName = windowClass;
	wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;	// (HBRUSH)(COLOR_WINDOW);

	roVerify(::RegisterClassW(&wc) != 0);

	// OpenGL requires WS_CLIPCHILDREN and WS_CLIPSIBLINGS, see page 657 of OpenGL SuperBible 4th edition
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	// In windowed mode, adjust width and height so that window will have the requested client area
	RECT rect = { 0, 0, width, height };
	::AdjustWindowRect(&rect, style, /*hasMenu*/false);
	width = (width == CW_USEDEFAULT ? width : rect.right - rect.left);
	height = (height == CW_USEDEFAULT ? height : rect.bottom - rect.top);

	hWnd = ::CreateWindowW(
		windowClass, L"Rhinoca unit test",
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL, NULL,
		hModule,
		NULL
	);

	::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	::ShowWindow(hWnd, true);
}

static void _initRenderDriver(ro::SubSystems& subSystems)
{
	GraphicsTestBase& self = *(GraphicsTestBase*)(subSystems.userData[0]);
	roRDriver* driver = roNewRenderDriver((char*)subSystems.userData[1], NULL);
	roRDriverContext* context = driver->newContext(driver);
	context->majorVersion = 2;
	context->minorVersion = 0;
	driver->initContext(context, self.hWnd);
	driver->useContext(context);
	driver->setViewport(0, 0, context->width, context->height, 0, 1);

	self.driver = subSystems.renderDriver = driver;
	self.context = subSystems.renderContext = context;
}

ro::Status GraphicsTestBase::initContext(const char* driverStr)
{
	ro::Status st;
	subSystems.userData.pushBack((roUserData)this);
	subSystems.userData.pushBack((roUserData)driverStr);
	subSystems.initRenderDriver = _initRenderDriver;
	st = subSystems.init(); if(!st) return st;

	vShader = driver->newShader();
	gShader = driver->newShader();
	pShader = driver->newShader();

	return ro::Status::ok;
}

bool GraphicsTestBase::keepRun()
{
	MSG msg = { 0 };
	if(::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		(void)TranslateMessage(&msg);
		(void)DispatchMessage(&msg);

		if(msg.message == WM_QUIT)
			return false;
	}

	subSystems.tick();

	return true;
}

const wchar_t* GraphicsTestBase::windowClass = L"Rhinoca unit test";
