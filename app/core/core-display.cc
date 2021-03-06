#include "core-display.h"
#include "logger.h"
#include "core-engine.h"
#include "core-settings.h"
#include "core-d3d.h"
#include "core-scene.h"

namespace
{
	CoreDisplay* g_coredisplay = nullptr;
}

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(g_coredisplay)
		return g_coredisplay->MsgProc(hwnd, msg, wParam, lParam);

	return 1;
}

CoreDisplay::CoreDisplay()
{
	g_coredisplay = this;
}

CoreDisplay::~CoreDisplay()
{

}

void CoreDisplay::StartupCoreDisplay(HINSTANCE hInstance)
{
	if (!core_engine_)
		EXCEPTION_TEXT("core_engine_ missed");
	app_instance_ = hInstance;
	InitMainWindow();
}

void CoreDisplay::InitMainWindow()
{
	CoreSettings* setting = core_engine_->GetSettings();
	int width = setting->GetVideoParam()->width;
	int height = setting->GetVideoParam()->height;

	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = app_instance_;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"D3DWndClassName";

	if (!RegisterClass(&wc))
		EXCEPTION_TEXT("register window class failed");

	RECT R = { 0, 0, width, height };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	m_hMainWnd = CreateWindow(L"D3DWndClassName", L"TinyStudio",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, app_instance_, 0);

	if (!m_hMainWnd)
	{
		HRESULT hr = GetLastError();
		EXCEPTION_TEXT("CreateWindow failed hr:0x%x", hr);
	}

	ShowWindow(m_hMainWnd, SW_SHOW);

	UpdateWindow(m_hMainWnd);
}

void CoreDisplay::OnResize()
{
	resize_flag_ = false;
	CoreD3D* d3d = core_engine_->GetD3D();
	d3d->WindowResized();
}

int CoreDisplay::RunLoop()
{
	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Sleep(10);
		}
	}
	return (int)msg.wParam;
}


LRESULT CoreDisplay::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			m_AppPaused = true;
		}
		else
		{
			m_AppPaused = false;
		}
		return 0;

	case WM_SIZE:

		if (core_engine_)
		{
			CoreSettingsData::Video video;
			memcpy(&video, core_engine_->GetSettings()->GetVideoParam(), sizeof(CoreSettingsData::Video));
			video.width = LOWORD(lParam);
			video.height = HIWORD(lParam);
			core_engine_->GetSettings()->UpdateGlobalVideo(&video);
		}

		if (wParam == SIZE_MINIMIZED)
		{
			m_AppPaused = true;
			m_Minimized = true;
			m_Maximized = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			m_AppPaused = false;
			m_Minimized = false;
			m_Maximized = true;
			resize_flag_ = true;
		}
		else if (wParam == SIZE_RESTORED)
		{
			if (m_Minimized)
			{
				m_AppPaused = false;
				m_Minimized = false;
				resize_flag_ = true;
			}
			else if (m_Maximized)
			{
				m_AppPaused = false;
				m_Maximized = false;
				resize_flag_ = true;
			}
			else if (m_Resizing)
			{
			}
			else
			{
				resize_flag_ = true;
			}
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		m_AppPaused = true;
		m_Resizing = true;
		return 0;
	case WM_EXITSIZEMOVE:
		m_AppPaused = false;
		m_Resizing = false;
		resize_flag_ = true;
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		return 0;
	case WM_MOUSEMOVE:
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CoreDisplay::RenderDisplay()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	CoreScene* scene = core_engine_->GetScene();

	if (resize_flag_)
		OnResize();

	d3d->RenderBegin();
	scene->RenderSources();
	d3d->RenderEnd();
}