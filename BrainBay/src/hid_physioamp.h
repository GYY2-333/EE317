/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: HID_PHYSIOAMP.H:  declarations for the PhysioAmp GP-8 USB HID interface

  The PhysioAmp GP-8 is a custom USB HID device (PIC18F4458 based) that delivers
  EEG signal data. Unlike the NIA (which is read via Raw Input), this device
  requires the host to actively send Output Reports for the handshake, so it is
  accessed through the Win32 SetupAPI + HidD/HidP interface.

  First version implements the SIGNAL mode only:
     host --> 0x80 (handshake request)
     dev  --> 64 byte response (77 62 11 00 ...)
     host --> 0x8F (start streaming)
     dev  ==> 64 byte data frames, one every 10ms

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#ifndef HID_PHYSIOAMP_H
#define HID_PHYSIOAMP_H

#include <windows.h>

// --- USB identification ------------------------------------------------------
#define PHYSIOAMP_VID          0x0EA9   // idVendor  (PhysioData)
#define PHYSIOAMP_PID          0x0FA8   // idProduct (PhysioAmp GP-8)

// --- HID report / frame layout ----------------------------------------------
#define PA_REPORT_LEN          64       // HID report payload length (bytes)
#define PA_DATA_OFFSET         4        // first sample byte within a data frame
#define PA_SAMPLES_PER_FRAME   30       // number of 16-bit samples per frame

// --- handshake commands (host -> device, first byte of Output Report) --------
#define PA_CMD_HANDSHAKE       0x80     // request handshake, device replies
#define PA_CMD_START           0x8F     // start streaming
#define PA_CMD_SWITCHMODE      0xF8     // toggle signal<->impedance (unused v1)

// --- handshake response signature (first 4 bytes of the 64 byte reply) -------
#define PA_RESP_SIG0           0x77
#define PA_RESP_SIG1           0x62
#define PA_RESP_SIG2           0x11
#define PA_RESP_SIG3           0x00

// --- signal mode timing ------------------------------------------------------
#define PA_SAMPLINGRATE        3000     // 30 samples per frame, one frame / 10ms

// --- public interface --------------------------------------------------------
BOOL ConnectPhysioAmp(HWND hDlg);
BOOL DisconnectPhysioAmp(void);

#endif // HID_PHYSIOAMP_H
