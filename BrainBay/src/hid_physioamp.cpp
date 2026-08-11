/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: HID_PHYSIOAMP.CPP:  PhysioAmp GP-8 USB HID interface (signal mode)

  Provides interfacing to the PhysioAmp GP-8 custom USB HID device
  (VID 0x0EA9 / PID 0x0FA8). The device is enumerated via the Win32 SetupAPI,
  opened with CreateFile and read/written through the HID class driver.

  Because the device requires the host to send Output Reports for the handshake
  (0x80 / 0x8F), it cannot be driven through the Raw Input API (as the NIA is).

  A dedicated reader thread performs the blocking ReadFile() on the HID device.
  Each 64 byte data frame carries 30 little-endian 16-bit samples (signal mode).
  In signal mode the six "channels" are just consecutive slices of one single
  ADC time series, so the 30 samples are re-ordered into a single channel time
  series: for every sample we store it in PACKET.buffer[0] and call
  process_packets(), which drives the EEG object's work() function - identical
  in spirit to how the NIA / Ganglion sources feed the signal processing chain.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
#include "brainBay.h"
#include "hid_physioamp.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

// --- module state ------------------------------------------------------------
static HANDLE  hPhysioAmp        = INVALID_HANDLE_VALUE;
static HANDLE  hReaderThread     = NULL;
static volatile BOOL readerDone  = FALSE;
static USHORT  inputReportLength  = PA_REPORT_LEN + 1;   // incl. report id byte
static USHORT  outputReportLength = PA_REPORT_LEN + 1;

static int  oldFrameNumber = -1;   // for lost-frame detection (byte 0)


/*----------------------------------------------------------------------------
  find_and_open_physioamp:  enumerate all HID devices, open the one whose
  attributes match PHYSIOAMP_VID / PHYSIOAMP_PID.  Returns TRUE on success and
  fills hPhysioAmp plus the report lengths.
----------------------------------------------------------------------------*/
static BOOL find_and_open_physioamp(void)
{
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL,
	                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (devInfo == INVALID_HANDLE_VALUE)
	{
		write_logfile("PhysioAmp: SetupDiGetClassDevs failed");
		return FALSE;
	}

	SP_DEVICE_INTERFACE_DATA ifData;
	ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL opened = FALSE;
	for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, idx, &ifData); idx++)
	{
		DWORD needed = 0;
		SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, NULL, 0, &needed, NULL);
		if (needed == 0) continue;

		PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
		    (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(needed);
		if (detail == NULL) continue;
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, detail, needed, NULL, NULL))
		{
			HANDLE h = CreateFile(detail->DevicePath,
			                      GENERIC_READ | GENERIC_WRITE,
			                      FILE_SHARE_READ | FILE_SHARE_WRITE,
			                      NULL, OPEN_EXISTING, 0, NULL);
			if (h != INVALID_HANDLE_VALUE)
			{
				HIDD_ATTRIBUTES attr;
				attr.Size = sizeof(HIDD_ATTRIBUTES);
				if (HidD_GetAttributes(h, &attr) &&
				    attr.VendorID  == PHYSIOAMP_VID &&
				    attr.ProductID == PHYSIOAMP_PID)
				{
					// query the actual report lengths from the caps
					PHIDP_PREPARSED_DATA preparsed = NULL;
					if (HidD_GetPreparsedData(h, &preparsed))
					{
						HIDP_CAPS caps;
						if (HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS)
						{
							if (caps.InputReportByteLength  > 0)
								inputReportLength  = caps.InputReportByteLength;
							if (caps.OutputReportByteLength > 0)
								outputReportLength = caps.OutputReportByteLength;
						}
						HidD_FreePreparsedData(preparsed);
					}
					hPhysioAmp = h;
					opened = TRUE;
					free(detail);
					break;
				}
				CloseHandle(h);
			}
		}
		free(detail);
	}

	SetupDiDestroyDeviceInfoList(devInfo);
	return opened;
}


/*----------------------------------------------------------------------------
  send_command:  send a single-byte handshake command as an Output Report.
  The report buffer is [ReportID=0][cmd][0..0], outputReportLength bytes.
----------------------------------------------------------------------------*/
static BOOL send_command(unsigned char cmd)
{
	if (hPhysioAmp == INVALID_HANDLE_VALUE) return FALSE;

	BYTE *buf = (BYTE*) calloc(outputReportLength, 1);
	if (buf == NULL) return FALSE;

	buf[0] = 0;      // report id
	buf[1] = cmd;    // command byte (payload byte 0)

	DWORD written = 0;
	BOOL ok = WriteFile(hPhysioAmp, buf, outputReportLength, &written, NULL);
	if (!ok || written != outputReportLength)
	{
		// fall back to the HID class helper (some stacks prefer this)
		ok = HidD_SetOutputReport(hPhysioAmp, buf, outputReportLength);
	}
	free(buf);
	return ok;
}


/*----------------------------------------------------------------------------
  read_response:  perform the handshake read and verify the 4-byte signature.
  Returns TRUE if a report with signature 77 62 11 00 was received.
----------------------------------------------------------------------------*/
static BOOL read_response(void)
{
	if (hPhysioAmp == INVALID_HANDLE_VALUE) return FALSE;

	BYTE *buf = (BYTE*) calloc(inputReportLength, 1);
	if (buf == NULL) return FALSE;

	DWORD read = 0;
	BOOL ok = ReadFile(hPhysioAmp, buf, inputReportLength, &read, NULL);

	// payload starts after the leading report-id byte
	BOOL valid = FALSE;
	if (ok && read >= 5)
	{
		if (buf[1] == PA_RESP_SIG0 && buf[2] == PA_RESP_SIG1 &&
		    buf[3] == PA_RESP_SIG2 && buf[4] == PA_RESP_SIG3)
			valid = TRUE;
	}
	free(buf);
	return valid;
}


/*----------------------------------------------------------------------------
  parse_frame:  decode one 64 byte data frame (payload, without report id).
  Signal mode: 30 little-endian 16-bit samples, re-ordered to a single channel.
----------------------------------------------------------------------------*/
static void parse_frame(const BYTE *payload)
{
	// lost-frame detection using the running frame number (byte 0)
	int frameNumber = payload[0];
	if (oldFrameNumber >= 0)
	{
		if (frameNumber != ((oldFrameNumber + 1) & 0xff))
			GLOBAL.syncloss++;
	}
	oldFrameNumber = frameNumber;

	if (TTY.read_pause || GLOBAL.loading) return;

	for (int s = 0; s < PA_SAMPLES_PER_FRAME; s++)
	{
		int off = PA_DATA_OFFSET + s * 2;
		unsigned int sample = (unsigned int) payload[off] |
		                      ((unsigned int) payload[off + 1] << 8);

		PACKET.buffer[0] = sample;
		process_packets();          // drives the EEG object's work()
	}
}


/*----------------------------------------------------------------------------
  PhysioAmpReadThread:  blocking reader loop.
----------------------------------------------------------------------------*/
static DWORD WINAPI PhysioAmpReadThread(LPVOID lpv)
{
	write_logfile("PhysioAmp reader thread running");

	BYTE *buf = (BYTE*) calloc(inputReportLength, 1);
	if (buf == NULL)
	{
		write_logfile("PhysioAmp: reader buffer allocation failed");
		return 0;
	}

	while (!readerDone)
	{
		DWORD read = 0;
		if (ReadFile(hPhysioAmp, buf, inputReportLength, &read, NULL))
		{
			if (read >= (DWORD)(PA_DATA_OFFSET + PA_SAMPLES_PER_FRAME * 2 + 1))
				parse_frame(buf + 1);   // skip the leading report-id byte
		}
		else
		{
			// device removed / error -> leave the loop
			if (!readerDone) write_logfile("PhysioAmp: ReadFile failed, closing reader");
			break;
		}
	}

	free(buf);
	write_logfile("PhysioAmp reader thread closed");
	return 0;
}


/*----------------------------------------------------------------------------
  ConnectPhysioAmp:  open the device, perform the signal-mode handshake and
  start the reader thread.
----------------------------------------------------------------------------*/
BOOL ConnectPhysioAmp(HWND hDlg)
{
	int sav_pause;

	write_logfile("Connecting PhysioAmp");

	(void)hDlg;   // handshake needs no dialog interaction for this device

	if (hPhysioAmp != INVALID_HANDLE_VALUE)
	{
		write_logfile("PhysioAmp already connected!");
		return TRUE;
	}

	sav_pause = TTY.read_pause;
	TTY.read_pause = 1;

	if (!find_and_open_physioamp())
	{
		write_logfile("No PhysioAmp found");
		TTY.read_pause = sav_pause;
		return FALSE;
	}

	// signal mode is the device default: 0x80 (handshake) -> 0x8F (start)
	if (!send_command(PA_CMD_HANDSHAKE))
	{
		write_logfile("PhysioAmp: could not send handshake");
		CloseHandle(hPhysioAmp);
		hPhysioAmp = INVALID_HANDLE_VALUE;
		TTY.read_pause = sav_pause;
		return FALSE;
	}

	if (!read_response())
	{
		write_logfile("PhysioAmp: invalid handshake response");
		CloseHandle(hPhysioAmp);
		hPhysioAmp = INVALID_HANDLE_VALUE;
		TTY.read_pause = sav_pause;
		return FALSE;
	}

	if (!send_command(PA_CMD_START))
	{
		write_logfile("PhysioAmp: could not send start command");
		CloseHandle(hPhysioAmp);
		hPhysioAmp = INVALID_HANDLE_VALUE;
		TTY.read_pause = sav_pause;
		return FALSE;
	}

	// launch the reader thread
	oldFrameNumber = -1;
	readerDone     = FALSE;
	hReaderThread  = CreateThread(NULL, 0, PhysioAmpReadThread, NULL, 0, NULL);
	if (hReaderThread == NULL)
	{
		write_logfile("PhysioAmp: could not create reader thread");
		CloseHandle(hPhysioAmp);
		hPhysioAmp = INVALID_HANDLE_VALUE;
		TTY.read_pause = sav_pause;
		return FALSE;
	}

	TTY.read_pause = sav_pause;
	write_logfile("PhysioAmp connected (signal mode)");
	return TRUE;
}


/*----------------------------------------------------------------------------
  DisconnectPhysioAmp:  stop the reader thread and close the device.
----------------------------------------------------------------------------*/
BOOL DisconnectPhysioAmp(void)
{
	write_logfile("Disconnecting PhysioAmp");

	readerDone = TRUE;

	if (hReaderThread != NULL)
	{
		// CancelSynchronousIo unblocks a pending ReadFile on the reader thread
		CancelSynchronousIo(hReaderThread);
		WaitForSingleObject(hReaderThread, 2000);
		CloseHandle(hReaderThread);
		hReaderThread = NULL;
	}

	if (hPhysioAmp != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hPhysioAmp);
		hPhysioAmp = INVALID_HANDLE_VALUE;
	}

	oldFrameNumber = -1;
	return TRUE;
}
