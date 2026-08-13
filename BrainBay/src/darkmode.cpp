/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: DARKMODE.CPP:  global dark-theme support for Win32 dialogs

  See darkmode.h for the rationale.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include "brainBay.h"
#include "darkmode.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

// text colors used across the dark dialogs
#define DM_TEXT_NORMAL   RGB(225, 225, 230)
#define DM_TEXT_DISABLED RGB(140, 140, 150)
#define DM_BK_DIALOG     RGB( 28,  28,  30)
#define DM_BK_EDIT       RGB( 44,  44,  48)

// subclass id (arbitrary but unique within this module)
#define DM_SUBCLASS_ID   0xDA2C

// ---------------------------------------------------------------------------
//  De-theme standard controls so WM_CTLCOLOR* takes effect and remove the
//  light classic-control frames. Called once per dialog on first paint.
// ---------------------------------------------------------------------------
static BOOL CALLBACK dm_theme_child(HWND hChild, LPARAM lParam)
{
	char cls[64];
	GetClassNameA(hChild, cls, sizeof(cls));

	// Buttons (push/check/radio/group) and combo boxes ignore WM_CTLCOLOR
	// while the visual style is active -> strip the theme from them.
	if (lstrcmpiA(cls, "Button")==0 ||
	    lstrcmpiA(cls, "ComboBox")==0 ||
	    lstrcmpiA(cls, "msctls_trackbar32")==0 ||
	    lstrcmpiA(cls, "ScrollBar")==0)
	{
		SetWindowTheme(hChild, L"", L"");
	}
	return TRUE;
}

// ---------------------------------------------------------------------------
//  Central subclass procedure: answer all WM_CTLCOLOR* with dark brushes.
// ---------------------------------------------------------------------------
static LRESULT CALLBACK DarkSubclassProc(HWND hWnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR uId, DWORD_PTR ref)
{
	switch (msg)
	{
	case WM_CTLCOLORDLG:
		return (LRESULT)(DRAW.brush_dlg_bg ? DRAW.brush_dlg_bg :
		                 (HBRUSH)GetStockObject(BLACK_BRUSH));

	case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wParam;
			HWND ctl = (HWND)lParam;
			SetBkMode(hdc, TRANSPARENT);
			// dim the text of disabled controls (e.g. read-only status fields)
			BOOL enabled = IsWindowEnabled(ctl);
			SetTextColor(hdc, enabled ? DM_TEXT_NORMAL : DM_TEXT_DISABLED);
			SetBkColor(hdc, DM_BK_DIALOG);
			return (LRESULT)(DRAW.brush_dlg_bg ? DRAW.brush_dlg_bg :
			                 (HBRUSH)GetStockObject(BLACK_BRUSH));
		}

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc, DM_TEXT_NORMAL);
			SetBkColor(hdc, DM_BK_EDIT);
			return (LRESULT)(DRAW.brush_dlg_edit ? DRAW.brush_dlg_edit :
			                 (HBRUSH)GetStockObject(BLACK_BRUSH));
		}

	case WM_CTLCOLORBTN:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc, DM_TEXT_NORMAL);
			SetBkColor(hdc, DM_BK_EDIT);
			return (LRESULT)(DRAW.brush_dlg_btn ? DRAW.brush_dlg_btn :
			                 (HBRUSH)GetStockObject(BLACK_BRUSH));
		}

	case WM_CTLCOLORSCROLLBAR:
		return (LRESULT)(DRAW.brush_dlg_edit ? DRAW.brush_dlg_edit :
		                 (HBRUSH)GetStockObject(BLACK_BRUSH));

	case WM_NCDESTROY:
		RemoveWindowSubclass(hWnd, DarkSubclassProc, uId);
		break;
	}
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
//  Public: install the dark subclass on a dialog window.
// ---------------------------------------------------------------------------
void InstallDarkSubclass(HWND hWnd)
{
	if (!hWnd) return;

	// avoid installing twice
	DWORD_PTR ref = 0;
	if (GetWindowSubclass(hWnd, DarkSubclassProc, DM_SUBCLASS_ID, &ref))
		return;

	SetWindowSubclass(hWnd, DarkSubclassProc, DM_SUBCLASS_ID, 0);

	// strip themes from buttons / combos / sliders so they honour our colors
	EnumChildWindows(hWnd, dm_theme_child, 0);

	// match the title bar to the dark client area
	EnableDarkTitleBar(hWnd);

	InvalidateRect(hWnd, NULL, TRUE);
}

// ---------------------------------------------------------------------------
//  Public: enable the DWM dark title bar (Windows 10 1809+).
//  Loaded dynamically so we don't hard-depend on dwmapi.lib / new SDK values.
// ---------------------------------------------------------------------------
void EnableDarkTitleBar(HWND hWnd)
{
	if (!hWnd) return;

	typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
	static PFN_DwmSetWindowAttribute pDwmSet = NULL;
	static int tried = 0;

	if (!tried)
	{
		tried = 1;
		HMODULE h = LoadLibraryA("dwmapi.dll");
		if (h) pDwmSet = (PFN_DwmSetWindowAttribute)GetProcAddress(h, "DwmSetWindowAttribute");
	}
	if (!pDwmSet) return;

	BOOL dark = TRUE;
	// 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 1903+), 19 = pre-1903 build.
	if (FAILED(pDwmSet(hWnd, 20, &dark, sizeof(dark))))
		pDwmSet(hWnd, 19, &dark, sizeof(dark));
}
