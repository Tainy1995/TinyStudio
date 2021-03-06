#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H

#include "dx-header.h"
#include "core-component-i.h"

class CoreDisplay : public ICoreComponent
{
public:
	CoreDisplay();
	virtual ~CoreDisplay();
	void StartupCoreDisplay(HINSTANCE hInstance);
	int RunLoop();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void RenderDisplay();
	HWND GetMainHwnd() { return m_hMainWnd; }

private:
	void InitMainWindow();
	void OnResize();

private:
	HINSTANCE app_instance_ = 0;
	HWND      m_hMainWnd = nullptr;        
	bool      m_AppPaused = false;
	bool      m_Minimized = false;
	bool      m_Maximized = false;
	bool      m_Resizing = false;
	bool		 resize_flag_ = false;
};

#endif