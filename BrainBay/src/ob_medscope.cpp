/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  OB_MEDSCOPE.CPP:  Medical-style multi-channel signal scope object

  Renders incoming signal channels as smooth glowing traces on a dark,
  clinical-monitor style grid using the modern GL helper layer (gl_modern.*).

  See ob_medscope.h for the class definition.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#include "brainBay.h"
#include "ob_medscope.h"
#include "gl_modern.h"
#include <math.h>

// ---------------------------------------------------------------------------
//  Shared GL resources (all medscope windows share the same compatibility
//  context family, so a single program/VBO set is sufficient).
// ---------------------------------------------------------------------------
static GLuint s_lineProg = 0;      // colored line/quad program
static GLuint s_lineVBO  = 0;
static GLuint s_lineVAO  = 0;
static int    s_medInited = 0;     // 0=untried,1=ok,-1=failed

// simple 2D program: position in clip space via uProj, flat color via uColor
static const char *MED_VS =
	"#version 120\n"
	"attribute vec2 aPos;\n"
	"uniform mat4 uProj;\n"
	"void main(){ gl_Position = uProj * vec4(aPos,0.0,1.0); }\n";

static const char *MED_FS =
	"#version 120\n"
	"uniform vec4 uColor;\n"
	"void main(){ gl_FragColor = uColor; }\n";

// clinical monitor palette (per-channel trace colors, phosphor-like)
static const float MED_CHNCOL[MEDSCOPE_MAXCHN][3] = {
	{0.20f, 1.00f, 0.45f},   // phosphor green
	{0.30f, 0.85f, 1.00f},   // cyan
	{1.00f, 0.85f, 0.25f},   // amber
	{1.00f, 0.45f, 0.55f},   // salmon
	{0.70f, 0.70f, 1.00f},   // periwinkle
	{0.55f, 1.00f, 0.80f},   // mint
	{1.00f, 0.65f, 1.00f},   // pink
	{0.85f, 0.95f, 0.55f},   // lime
};

static void med_init_gl(void)
{
	if (s_medInited != 0) return;
	if (!ensureGladLoaded()) { s_medInited = -1; return; }
	s_lineProg = glmBuildProgram(MED_VS, MED_FS);
	if (!s_lineProg) { s_medInited = -1; return; }

	if (glm_GenVertexArrays) { glm_GenVertexArrays(1,&s_lineVAO); }
	glm_GenBuffers(1, &s_lineVBO);
	s_medInited = 1;
}

// upload a vertex array (interleaved x,y floats) and draw with the given mode
static void med_draw_array(const float *verts, int count, GLenum mode,
                           float r, float g, float b, float a)
{
	if (count<=0) return;
	if (s_lineVAO && glm_BindVertexArray) glm_BindVertexArray(s_lineVAO);
	glm_BindBuffer(GL_ARRAY_BUFFER, s_lineVBO);
	glm_BufferData(GL_ARRAY_BUFFER, sizeof(float)*2*count, verts, GL_STREAM_DRAW);
	GLint aPos = glm_GetAttribLocation(s_lineProg, "aPos");
	if (aPos>=0){ glm_EnableVertexAttribArray(aPos);
		glm_VertexAttribPointer(aPos,2,GL_FLOAT,GL_FALSE,0,(void*)0); }
	GLint uCol = glm_GetUniformLocation(s_lineProg, "uColor");
	if (uCol>=0) glm_Uniform4f(uCol, r,g,b,a);
	glDrawArrays(mode, 0, count);
	if (aPos>=0) glm_DisableVertexAttribArray(aPos);
}

// ---------------------------------------------------------------------------
//  Main GL draw
// ---------------------------------------------------------------------------
void draw_medscope(MEDSCOPEOBJ * st)
{
	RECT rc; GetClientRect(st->displayWnd, &rc);
	int W = rc.right, H = rc.bottom;
	if (W<=0||H<=0) return;

	glViewport(0,0,W,H);

	// clinical dark background
	glClearColor(0.03f, 0.05f, 0.06f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	med_init_gl();
	if (s_medInited != 1)
	{
		// modern GL unavailable: leave the cleared dark window (graceful)
		return;
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	glm_UseProgram(s_lineProg);
	// orthographic projection in pixel space (0..W, 0..H), origin top-left
	glmMat4 proj = glmOrtho(0.0f,(float)W,(float)H,0.0f,-1.0f,1.0f);
	GLint uProj = glm_GetUniformLocation(s_lineProg,"uProj");
	if (uProj>=0) glm_UniformMatrix4fv(uProj,1,GL_FALSE,proj.m);

	int chn = st->channels; if (chn<1) chn=1; if (chn>MEDSCOPE_MAXCHN) chn=MEDSCOPE_MAXCHN;
	float laneH = (float)H / chn;

	// --- grid (minor + major) ---------------------------------------------
	{
		static float g[4*64];
		int n=0;
		// vertical time divisions (10 columns)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		for (int i=1;i<10;i++){
			float x=(float)W*i/10.0f;
			g[n*2]=x; g[n*2+1]=0; n++;
			g[n*2]=x; g[n*2+1]=(float)H; n++;
		}
		med_draw_array(g, n, GL_LINES, 0.15f,0.30f,0.30f,0.5f);
		// horizontal lane separators + mid lines
		n=0;
		for (int c=0;c<=chn;c++){
			float y=laneH*c;
			g[n*2]=0; g[n*2+1]=y; n++;
			g[n*2]=(float)W; g[n*2+1]=y; n++;
		}
		med_draw_array(g, n, GL_LINES, 0.20f,0.40f,0.40f,0.6f);
		// per-lane mid (baseline) lines - dimmer
		n=0;
		for (int c=0;c<chn;c++){
			float y=laneH*c + laneH*0.5f;
			g[n*2]=0; g[n*2+1]=y; n++;
			g[n*2]=(float)W; g[n*2+1]=y; n++;
		}
		med_draw_array(g, n, GL_LINES, 0.12f,0.25f,0.25f,0.5f);
	}

	// --- traces ------------------------------------------------------------
	int total = st->histcount;
	if (total > MEDSCOPE_HISTLEN) total = MEDSCOPE_HISTLEN;
	if (total > W) total = W;                  // one sample per pixel column max

	if (total >= 2)
	{
		static float line[MEDSCOPE_HISTLEN*2];
		float glow = st->glowlevel/100.0f;

		glBlendFunc(GL_SRC_ALPHA, GL_ONE);     // additive glow
		for (int c=0;c<chn;c++)
		{
			float midY = laneH*c + laneH*0.5f;
			float amp  = (laneH*0.42f) * st->gain;
			int n=0;
			for (int i=0;i<total;i++)
			{
				// walk back from newest sample
				int idx = st->histpos - total + i;
				while (idx<0) idx += MEDSCOPE_HISTLEN;
				idx %= MEDSCOPE_HISTLEN;
				float v = st->hist[c][idx];
				float x = (float)W * i/(float)(total-1);
				float y = midY - v*amp;
				if (y<laneH*c) y=laneH*c;
				if (y>laneH*(c+1)) y=laneH*(c+1);
				line[n*2]=x; line[n*2+1]=y; n++;
			}
			float r=MED_CHNCOL[c][0], g=MED_CHNCOL[c][1], b=MED_CHNCOL[c][2];

			// soft glow underlay: draw a few wider, dim passes
			if (glow>0.01f)
			{
				med_draw_array(line, n, GL_LINE_STRIP, r,g,b, 0.20f*glow);
				med_draw_array(line, n, GL_LINE_STRIP, r,g,b, 0.12f*glow);
			}
			// crisp core trace
			med_draw_array(line, n, GL_LINE_STRIP, r,g,b, 1.0f);
		}
	}

	glm_UseProgram(0);
}

// ---------------------------------------------------------------------------
//  GL window creation
// ---------------------------------------------------------------------------
HWND MEDSCOPEOBJ::create_Med_Window(int l,int r,int t,int b)
{
	HWND hWnd;
	HDC  hDC;
	PIXELFORMATDESCRIPTOR pfd;
	int nPixelFormat;

	if ((hWnd = CreateWindow("MedScopeClass", "Medical Signal Scope",
	     WS_CLIPSIBLINGS | WS_CHILD | WS_CAPTION | WS_THICKFRAME | WS_VISIBLE,
	     l, t, r-l, b-t, ghWndMain, NULL, hInst, NULL)))
	{
		ShowWindow(hWnd, 1);
		UpdateWindow(hWnd);

		memset(&pfd, 0, sizeof(pfd));
		pfd.nSize      = sizeof(pfd);
		pfd.nVersion   = 1;
		pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 24;
		pfd.cDepthBits = 16;
		pfd.iLayerType = PFD_MAIN_PLANE;

		hDC = GetDC(hWnd);
		if (hDC==NULL) return(0);
		nPixelFormat = ChoosePixelFormat(hDC, &pfd);
		SetPixelFormat(hDC, nPixelFormat, &pfd);

		GLRC_Med = wglCreateContext(hDC);
		wglMakeCurrent(hDC, GLRC_Med);
		ensureGladLoaded();              // load modern GL once a context is current
		glClearColor(0.03f, 0.05f, 0.06f, 1.0f);
		wglMakeCurrent(0,0);
		ReleaseDC(hWnd, hDC);

		InvalidateRect(hWnd, NULL, TRUE);
		return(hWnd);
	}
	return(0);
}

// ---------------------------------------------------------------------------
//  Object methods
// ---------------------------------------------------------------------------
MEDSCOPEOBJ::MEDSCOPEOBJ(int num) : BASE_CL()
{
	outports = 0;
	inports  = MEDSCOPE_MAXCHN;
	width    = 75;

	for (int i=0;i<MEDSCOPE_MAXCHN;i++)
	{
		sprintf(in_ports[i].in_name, "Chn%d", i+1);
		sprintf(in_ports[i].in_desc, "Signal channel %d", i+1);
		in_ports[i].get_range = TRUE;
		curval[i] = INVALID_VALUE;
		have[i]   = 0;
		for (int j=0;j<MEDSCOPE_HISTLEN;j++) hist[i][j]=0.0f;
	}

	channels  = 1;
	histpos   = 0;
	histcount = 0;
	timebase  = 5.0f;
	gain      = 1.0f;
	sweepmode = 0;
	glowlevel = 60;

	left=50; top=50; right=650; bottom=350;
	GLRC_Med = NULL;

	displayWnd = create_Med_Window(left, right, top, bottom);
}

void MEDSCOPEOBJ::make_dialog(void)
{
	display_toolbox(hDlg = CreateDialog(hInst, (LPCTSTR)IDD_MEDSCOPEBOX,
	                ghWndStatusbox, (DLGPROC)MedScopeDlgHandler));
}

void MEDSCOPEOBJ::load(HANDLE hFile)
{
	load_object_basics(this);
	load_property("channels",  P_INT,   &channels);
	load_property("timebase",  P_FLOAT, &timebase);
	load_property("gain",      P_FLOAT, &gain);
	load_property("sweepmode", P_INT,   &sweepmode);
	load_property("glowlevel", P_INT,   &glowlevel);
	load_property("left",   P_INT, &left);
	load_property("top",    P_INT, &top);
	load_property("right",  P_INT, &right);
	load_property("bottom", P_INT, &bottom);
	if (channels<1) channels=1;
	if (channels>MEDSCOPE_MAXCHN) channels=MEDSCOPE_MAXCHN;
	if (displayWnd)
		MoveWindow(displayWnd, left, top, right-left, bottom-top, TRUE);
}

void MEDSCOPEOBJ::save(HANDLE hFile)
{
	save_object_basics(hFile, this);
	save_property(hFile, "channels",  P_INT,   &channels);
	save_property(hFile, "timebase",  P_FLOAT, &timebase);
	save_property(hFile, "gain",      P_FLOAT, &gain);
	save_property(hFile, "sweepmode", P_INT,   &sweepmode);
	save_property(hFile, "glowlevel", P_INT,   &glowlevel);
	save_property(hFile, "left",   P_INT, &left);
	save_property(hFile, "top",    P_INT, &top);
	save_property(hFile, "right",  P_INT, &right);
	save_property(hFile, "bottom", P_INT, &bottom);
}

void MEDSCOPEOBJ::incoming_data(int port, float value)
{
	if (port<0 || port>=MEDSCOPE_MAXCHN) return;
	if (value != INVALID_VALUE)
	{
		// normalize into [-1..1] using the input port range
		float lo = in_ports[port].in_min, hi = in_ports[port].in_max;
		float span = (hi-lo);
		float nv = (span!=0.0f) ? (2.0f*(value-lo)/span - 1.0f) : 0.0f;
		curval[port] = nv;
		have[port] = 1;
		if (port+1 > channels && port < MEDSCOPE_MAXCHN) channels = port+1;
	}
}

void MEDSCOPEOBJ::work(void)
{
	// append one sample per channel to the ring buffer each work() cycle
	for (int c=0;c<MEDSCOPE_MAXCHN;c++)
	{
		float v = (curval[c]!=INVALID_VALUE) ? curval[c] : 0.0f;
		hist[c][histpos] = v;
	}
	histpos = (histpos+1) % MEDSCOPE_HISTLEN;
	if (histcount < MEDSCOPE_HISTLEN) histcount++;

	if (!TIMING.draw_update && displayWnd)
		InvalidateRect(displayWnd, NULL, FALSE);
}

MEDSCOPEOBJ::~MEDSCOPEOBJ()
{
	if (GLRC_Med) Shutdown_GL(GLRC_Med);
	if (displayWnd) DestroyWindow(displayWnd);
}

// ---------------------------------------------------------------------------
//  GL window message handler
// ---------------------------------------------------------------------------
LRESULT CALLBACK MedScopeWndHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hDC;
	MEDSCOPEOBJ * st = NULL;

	// locate the object that owns this window
	for (int i=0;i<GLOBAL.objects;i++)
		if (objects[i] && objects[i]->type==OB_MEDSCOPE &&
		    ((MEDSCOPEOBJ*)objects[i])->displayWnd==hWnd)
		{ st=(MEDSCOPEOBJ*)objects[i]; break; }

	switch (message)
	{
	case WM_PAINT:
		if (st)
		{
			hDC = BeginPaint(st->displayWnd, &ps);
			if (wglMakeCurrent(hDC, st->GLRC_Med))
			{
				draw_medscope(st);
				SwapBuffers(hDC);
				wglMakeCurrent(0,0);
			}
			EndPaint(st->displayWnd, &ps);
		}
		return 0;

	case WM_MOVE:
	case WM_SIZE:
		if (st)
		{
			WINDOWPLACEMENT wndpl;
			GetWindowPlacement(st->displayWnd, &wndpl);
			if (GLOBAL.locksession)
			{
				wndpl.rcNormalPosition.top=st->top;
				wndpl.rcNormalPosition.left=st->left;
				wndpl.rcNormalPosition.right=st->right;
				wndpl.rcNormalPosition.bottom=st->bottom;
				SetWindowPlacement(st->displayWnd, &wndpl);
			}
			else
			{
				st->top=wndpl.rcNormalPosition.top;
				st->left=wndpl.rcNormalPosition.left;
				st->right=wndpl.rcNormalPosition.right;
				st->bottom=wndpl.rcNormalPosition.bottom;
			}
			InvalidateRect(hWnd,NULL,FALSE);
		}
		break;

	case WM_CLOSE:
		return 0;   // don't allow closing the window directly
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// ---------------------------------------------------------------------------
//  Settings dialog handler
// ---------------------------------------------------------------------------
LRESULT CALLBACK MedScopeDlgHandler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	MEDSCOPEOBJ * st = (MEDSCOPEOBJ *) actobject;
	if ((st==NULL) || (st->type!=OB_MEDSCOPE)) return FALSE;

	char sztemp[40];

	switch (message)
	{
	case WM_INITDIALOG:
		SetScrollRange(GetDlgItem(hDlg,IDC_MED_CHANNELS), SB_CTL, 1, MEDSCOPE_MAXCHN, TRUE);
		SetScrollPos  (GetDlgItem(hDlg,IDC_MED_CHANNELS), SB_CTL, st->channels, TRUE);
		SetScrollRange(GetDlgItem(hDlg,IDC_MED_GLOW), SB_CTL, 0, 100, TRUE);
		SetScrollPos  (GetDlgItem(hDlg,IDC_MED_GLOW), SB_CTL, st->glowlevel, TRUE);
		sprintf(sztemp,"%.2f",st->gain);     SetDlgItemText(hDlg,IDC_MED_GAIN,sztemp);
		sprintf(sztemp,"%.2f",st->timebase); SetDlgItemText(hDlg,IDC_MED_TIMEBASE,sztemp);
		CheckDlgButton(hDlg, IDC_MED_SWEEP, st->sweepmode?BST_CHECKED:BST_UNCHECKED);
		sprintf(sztemp,"%d",st->channels);   SetDlgItemText(hDlg,IDC_MED_CHNLABEL,sztemp);
		sprintf(sztemp,"%d",st->glowlevel);  SetDlgItemText(hDlg,IDC_MED_GLOWLABEL,sztemp);
		return TRUE;

	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		break;

	case WM_HSCROLL:
		{
			HWND bar = (HWND)lParam;
			int pos = GetScrollPos(bar, SB_CTL);
			int code = LOWORD(wParam);
			if (code==SB_LINELEFT||code==SB_PAGELEFT)  pos--;
			if (code==SB_LINERIGHT||code==SB_PAGERIGHT) pos++;
			if (code==SB_THUMBTRACK||code==SB_THUMBPOSITION) pos=HIWORD(wParam);
			if (bar==GetDlgItem(hDlg,IDC_MED_CHANNELS))
			{
				if (pos<1) pos=1; if (pos>MEDSCOPE_MAXCHN) pos=MEDSCOPE_MAXCHN;
				st->channels=pos; SetScrollPos(bar,SB_CTL,pos,TRUE);
				sprintf(sztemp,"%d",pos); SetDlgItemText(hDlg,IDC_MED_CHNLABEL,sztemp);
			}
			else if (bar==GetDlgItem(hDlg,IDC_MED_GLOW))
			{
				if (pos<0) pos=0; if (pos>100) pos=100;
				st->glowlevel=pos; SetScrollPos(bar,SB_CTL,pos,TRUE);
				sprintf(sztemp,"%d",pos); SetDlgItemText(hDlg,IDC_MED_GLOWLABEL,sztemp);
			}
			if (st->displayWnd) InvalidateRect(st->displayWnd,NULL,FALSE);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_MED_GAIN:
			if (HIWORD(wParam)==EN_KILLFOCUS)
			{
				GetDlgItemText(hDlg,IDC_MED_GAIN,sztemp,sizeof(sztemp));
				st->gain=(float)atof(sztemp);
				if (st->gain<=0.0f) st->gain=1.0f;
				if (st->displayWnd) InvalidateRect(st->displayWnd,NULL,FALSE);
			}
			break;
		case IDC_MED_TIMEBASE:
			if (HIWORD(wParam)==EN_KILLFOCUS)
			{
				GetDlgItemText(hDlg,IDC_MED_TIMEBASE,sztemp,sizeof(sztemp));
				st->timebase=(float)atof(sztemp);
				if (st->timebase<=0.0f) st->timebase=5.0f;
			}
			break;
		case IDC_MED_SWEEP:
			st->sweepmode = IsDlgButtonChecked(hDlg,IDC_MED_SWEEP)?1:0;
			if (st->displayWnd) InvalidateRect(st->displayWnd,NULL,FALSE);
			break;
		}
		break;
	}
	return FALSE;
}

