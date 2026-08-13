/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: DARKMODE.H:  global dark-theme support for Win32 dialogs

  BrainBay's main window and signal-flow canvas already use a dark, LabVIEW
  style theme, but the ~87 property/settings dialogs were still the classic
  light Win32 look. This module retrofits a consistent dark theme onto all of
  them with minimal intrusion:

    - InstallDarkSubclass() subclasses a dialog window so that all
      WM_CTLCOLOR* messages are intercepted centrally and answered with the
      process-wide dark brushes (DRAW.brush_dlg_*), without touching any of the
      individual dialog procedures.
    - EnableDarkTitleBar() asks the DWM to render the window's title bar in
      dark mode (Windows 10 1809+), so the caption matches the dark client.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#ifndef DARKMODE_H
#define DARKMODE_H

#include <windows.h>

// Install the central dark WM_CTLCOLOR* subclass on a dialog (or any window
// that hosts standard controls). Safe to call once per window; the subclass
// removes itself on WM_NCDESTROY.
void InstallDarkSubclass(HWND hWnd);

// Ask the DWM to draw this window's title bar in dark mode (no-op on OS
// versions that don't support it).
void EnableDarkTitleBar(HWND hWnd);

#endif // DARKMODE_H
