/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  OB_MEDSCOPE.H:  Medical-style multi-channel signal scope object

  A modern-OpenGL signal display that renders incoming channels as smooth,
  anti-aliased glowing traces on a dark, monitor-style grid with per-channel
  lanes, time/amplitude ticks and unit/gain labels - the look of a clinical
  patient monitor / EEG amplifier display.

  Rendering uses the compatibility GL context + GLSL helpers in gl_modern.*.
  If modern GL is unavailable it degrades to a simple fixed-function line draw.

-----------------------------------------------------------------------------*/

#ifndef OB_MEDSCOPE_H
#define OB_MEDSCOPE_H

#include "brainBay.h"

#define MEDSCOPE_MAXCHN     8         // max displayed channels
#define MEDSCOPE_HISTLEN    2048      // samples kept per channel (ring buffer)

class MEDSCOPEOBJ : public BASE_CL
{
  public:
	int    channels;                          // active input channels
	float  hist[MEDSCOPE_MAXCHN][MEDSCOPE_HISTLEN];
	int    histpos;                           // write cursor in ring buffer
	int    histcount;                         // valid samples so far
	float  curval[MEDSCOPE_MAXCHN];           // latest value per channel
	int    have[MEDSCOPE_MAXCHN];             // channel received data this frame

	float  timebase;                          // seconds shown across width
	float  gain;                              // amplitude scale factor
	int    sweepmode;                         // 0=scroll, 1=sweep
	int    glowlevel;                         // 0..100 glow intensity

	HGLRC  GLRC_Med;                          // this object's GL context
	int    left, right, top, bottom;          // window placement

	MEDSCOPEOBJ(int num);
	HWND create_Med_Window(int left,int right,int top,int bottom);
	void make_dialog(void);
	void load(HANDLE hFile);
	void save(HANDLE hFile);
	void incoming_data(int port, float value);
	void work(void);
	~MEDSCOPEOBJ();
};

LRESULT CALLBACK MedScopeWndHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MedScopeDlgHandler(HWND, UINT, WPARAM, LPARAM);
void draw_medscope(MEDSCOPEOBJ * st);

#endif // OB_MEDSCOPE_H
