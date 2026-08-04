/* -----------------------------------------------------------------------------
   BrainBay  -  OpenSource Biofeedback Software

  MODULE: DRAW.CPP: this Module provides global accessible Drawing - Functions.

  init_draw: Creates Pens and Brushes, and a font for GDI-use
  draw_objects: draws the current objects and object-links to the Main-Window
  LoadBMP: load a bitmap from a file to a openGL-surface, using SDL-functions

  LoadGLTextures: Converts a bitmap to a texture for GL-use
  Size_GL:  updates the OGL-viewport when a resize has occurred

  ->OGL-drawing is currently used for the FFT-Specra-Displays and the Animation Window

  Contributors:  many thanks go to Jeff Molofee (NeHe) for his great OGL-tutorial
				 Web Site: nehe.gamedev.net

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.


 --------------------------------------------------------------------------------------*/

#include "brainBay.h"
#include "ob_evaluator.h"
#include "ob_compare.h"



#define PARTICLEBITMAP "particle.bmp"


GLuint	base;						// Base Display List For The Font Set
GLuint	texture[1]={0};					// Storage For Our Particle Texture
int SX,SY;


void init_draw(void)
{
    HDC hdc;
	// ------------------------------------------------------------------
	//  LabVIEW-style block-diagram color scheme
	// ------------------------------------------------------------------
	//  Canvas   : light neutral gray with a fine dotted grid
	//  Modules  : light 3D-beveled panels with a colored title strip
	//  Wires    : thick orange data wires with a soft dark outline,
	//             routed as right-angle (orthogonal) segments
	//  Ports    : small beveled terminals (green = input, orange = output)
	// ------------------------------------------------------------------

	// --- legacy pens/brushes kept for compatibility with other modules ---
	DRAW.pen_white    = CreatePen(PS_SOLID, 1, PALETTERGB(255, 255, 255));
	DRAW.pen_blue     = CreatePen(PS_SOLID, 1, PALETTERGB(120, 120, 128));  // module border
	DRAW.pen_ltblue   = CreatePen(PS_SOLID, 1, PALETTERGB(160, 160, 170));
	DRAW.pen_red      = CreatePen(PS_SOLID, 3, PALETTERGB(232, 145,  20));  // orange data wire
	DRAW.brush_blue   = CreateSolidBrush(PALETTERGB( 47, 104, 168));        // title bar blue
	DRAW.brush_white  = CreateSolidBrush(PALETTERGB(255, 255, 255));
	DRAW.brush_orange = CreateSolidBrush(PALETTERGB(240, 150,  30));        // output port orange
	DRAW.brush_ltorange = CreateSolidBrush(PALETTERGB(255, 255, 255));
	DRAW.brush_yellow = CreateSolidBrush(PALETTERGB( 70, 180,  90));        // input port green
	DRAW.brush_ltgreen = CreateSolidBrush(PALETTERGB(228, 230, 235));       // module body fill

	// --- new LabVIEW-style resources ---
	DRAW.pen_grid       = CreatePen(PS_SOLID, 1, PALETTERGB(205, 208, 214)); // minor grid dots
	DRAW.pen_gridmajor  = CreatePen(PS_SOLID, 1, PALETTERGB(188, 192, 200)); // major grid lines
	DRAW.pen_wire_shadow= CreatePen(PS_SOLID, 5, PALETTERGB(150,  92,  10)); // wire outline
	DRAW.pen_wire_sel   = CreatePen(PS_SOLID, 3, PALETTERGB( 60, 120, 215)); // selected wire (blue)
	DRAW.pen_shadow     = CreatePen(PS_SOLID, 1, PALETTERGB(120, 122, 128)); // dark bevel
	DRAW.pen_hilite     = CreatePen(PS_SOLID, 1, PALETTERGB(255, 255, 255)); // light bevel
	DRAW.pen_portedge   = CreatePen(PS_SOLID, 1, PALETTERGB( 60,  60,  66)); // terminal outline
	DRAW.brush_canvas   = CreateSolidBrush(PALETTERGB(236, 238, 242));       // canvas bg
	DRAW.brush_body     = CreateSolidBrush(PALETTERGB(230, 232, 237));       // module body
	DRAW.brush_title    = CreateSolidBrush(PALETTERGB( 47, 104, 168));       // title strip
	DRAW.brush_title2   = CreateSolidBrush(PALETTERGB( 33,  78, 132));       // title strip (dark)
	DRAW.brush_shadow   = CreateSolidBrush(PALETTERGB(200, 202, 208));       // module shadow

	DRAW.particles=0;


    hdc = GetDC(NULL);
	if (GLOBAL.os_version==1)  DRAW.scaleFontHeight = -MulDiv(7, GetDeviceCaps(hdc, LOGPIXELSY), 90);
	else DRAW.scaleFontHeight = -MulDiv(7, GetDeviceCaps(hdc, LOGPIXELSY), 75);

    ReleaseDC(NULL, hdc);
    if (!(DRAW.scaleFont = CreateFont(DRAW.scaleFontHeight, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial")))
        report_error("Font creation failed!");
	if (!(DRAW.mediumFont = CreateFont(30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial")))
        report_error("Font creation failed!");
	// bold title font (slightly larger than the port label font)
	if (!(DRAW.titleFont = CreateFont(DRAW.scaleFontHeight, 0, 0, 0, FW_BOLD, 0, 0, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI")))
		DRAW.titleFont = DRAW.scaleFont;
}

// ---------------------------------------------------------------------------
//  gradient_fill_v : simple top-to-bottom vertical gradient using GDI
// ---------------------------------------------------------------------------
static void gradient_fill_v(HDC hdc, int x, int y, int w, int h,
							COLORREF top, COLORREF bot)
{
	if (h <= 0 || w <= 0) return;
	int rT = GetRValue(top), gT = GetGValue(top), bT = GetBValue(top);
	int rB = GetRValue(bot), gB = GetGValue(bot), bB = GetBValue(bot);
	for (int i = 0; i < h; i++)
	{
		int r = rT + (rB - rT) * i / h;
		int g = gT + (gB - gT) * i / h;
		int b = bT + (bB - bT) * i / h;
		HBRUSH hb = CreateSolidBrush(RGB(r, g, b));
		RECT rc = { x, y + i, x + w, y + i + 1 };
		FillRect(hdc, &rc, hb);
		DeleteObject(hb);
	}
}

// ---------------------------------------------------------------------------
//  draw_grid : LabVIEW-style block-diagram grid (dotted minor + major lines)
// ---------------------------------------------------------------------------
static void draw_grid(HDC hdc, const RECT* rc)
{
	const int minor = 12;   // spacing of minor grid
	const int major = 60;   // spacing of major grid

	// origin follows scroll so the grid appears fixed to the canvas
	int ox = ((SX % minor) + minor) % minor;
	int oy = ((SY % minor) + minor) % minor;

	// minor grid : single pixels (dots)
	HPEN old = (HPEN)SelectObject(hdc, DRAW.pen_grid);
	for (int y = rc->top + oy; y < rc->bottom; y += minor)
		for (int x = rc->left + ox; x < rc->right; x += minor)
			SetPixel(hdc, x, y, PALETTERGB(200, 203, 210));

	// major grid : faint full lines
	int mox = ((SX % major) + major) % major;
	int moy = ((SY % major) + major) % major;
	SelectObject(hdc, DRAW.pen_gridmajor);
	for (int x = rc->left + mox; x < rc->right; x += major)
	{ MoveToEx(hdc, x, rc->top, NULL); LineTo(hdc, x, rc->bottom); }
	for (int y = rc->top + moy; y < rc->bottom; y += major)
	{ MoveToEx(hdc, rc->left, y, NULL); LineTo(hdc, rc->right, y); }
	SelectObject(hdc, old);
}

void draw_object(HDC hdc, WORD t)
{
	int x = SX + objects[t]->xPos;
	int y = SY + objects[t]->yPos;
	int w = objects[t]->width;
	int h = objects[t]->height;
	int titleH = 17;
	if (titleH > h-2) titleH = h-2;

	// --- drop shadow (offset rounded rect) ---
	{
		HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
		HBRUSH ob = (HBRUSH)SelectObject(hdc, DRAW.brush_shadow);
		RoundRect(hdc, x+3, y+4, x+w+3, y+h+4, 9, 9);
		SelectObject(hdc, op);
		SelectObject(hdc, ob);
	}

	// --- module body (light panel, rounded) ---
	SelectObject(hdc, DRAW.pen_shadow);
	SelectObject(hdc, DRAW.brush_body);
	RoundRect(hdc, x, y, x+w, y+h, 9, 9);

	// --- title strip (blue vertical gradient) ---
	//  clip to the rounded top so the gradient respects the corners
	{
		HRGN clip = CreateRoundRectRgn(x, y, x+w+1, y+h+1, 9, 9);
		SelectClipRgn(hdc, clip);
		COLORREF tTop = RGB(72, 132, 200);
		COLORREF tBot = RGB(33,  78, 132);
		if (objects[t] == actobject) { tTop = RGB(96, 158, 224); tBot = RGB(46, 100, 164); }
		gradient_fill_v(hdc, x+1, y+1, w-2, titleH, tTop, tBot);
		// thin separator under the title strip
		HPEN sp = (HPEN)SelectObject(hdc, DRAW.pen_shadow);
		MoveToEx(hdc, x+1, y+titleH, NULL); LineTo(hdc, x+w-1, y+titleH);
		SelectObject(hdc, sp);
		SelectClipRgn(hdc, NULL);
		DeleteObject(clip);
	}

	// --- 3D bevels on the body ---
	HPEN hPenOld = (HPEN)SelectObject(hdc, DRAW.pen_hilite);
	MoveToEx(hdc, x+2, y+h-3, NULL); LineTo(hdc, x+2, y+titleH+1); LineTo(hdc, x+w-3, y+titleH+1);
	SelectObject(hdc, DRAW.pen_shadow);
	MoveToEx(hdc, x+2, y+h-2, NULL); LineTo(hdc, x+w-2, y+h-2); LineTo(hdc, x+w-2, y+titleH);
	SelectObject(hdc, hPenOld);

	// --- selection frame (dashed blue marching border) ---
	if (objects[t] == actobject)
	{
		LOGBRUSH lb; lb.lbStyle = BS_SOLID; lb.lbColor = RGB(40, 110, 210); lb.lbHatch = 0;
		HPEN hSel = ExtCreatePen(PS_GEOMETRIC | PS_DOT | PS_ENDCAP_FLAT, 2, &lb, 0, NULL);
		HPEN oldp = (HPEN)SelectObject(hdc, hSel);
		SelectObject(hdc, GetStockObject(NULL_BRUSH));
		RoundRect(hdc, x-4, y-4, x+w+4, y+h+4, 12, 12);
		SelectObject(hdc, oldp);
		DeleteObject(hSel);
	}
}


// ---------------------------------------------------------------------------
//  draw_wire : orthogonal (right-angle) LabVIEW-style wire from (x0,y0)->(x1,y1)
//              routed with a short horizontal stub + vertical + horizontal.
// ---------------------------------------------------------------------------
static void wire_path(HDC hdc, int x0, int y0, int x1, int y1)
{
	int stub = 10;
	int midx;
	if (x1 - x0 > 2*stub) midx = (x0 + x1) / 2;   // route the vertical in the middle
	else midx = x0 + stub;                         // going backwards: stub out first

	MoveToEx(hdc, x0, y0, NULL);
	LineTo(hdc, midx, y0);
	LineTo(hdc, midx, y1);
	LineTo(hdc, x1, y1);
}

void draw_connections(HDC hdc, WORD t)
{
	int i,k;

	int oldjoin = SetBkMode(hdc, TRANSPARENT);
	for (i=0;objects[t]->out[i].from_port!=-1;i++)
	{
		if (objects[t]->outports > objects[t]->out[i].from_port) {
			int x0 = SX + objects[t]->xPos + objects[t]->width - 4;
			int y0 = SY + objects[t]->yPos + CON_START + objects[t]->out[i].from_port * CON_HEIGHT;
			k = objects[t]->out[i].to_object;
			if ((GLOBAL.objects > k))
			{
				int x1, y1;
				if (objects[t]->out[i].to_port != -1)
				{
					x1 = SX + objects[k]->xPos + 4;
					y1 = SY + objects[k]->yPos + CON_START + objects[t]->out[i].to_port * CON_HEIGHT;
				}
				else { x1 = SX + GLOBAL.tx; y1 = SY + GLOBAL.ty; }

				int selected = (&(objects[t]->out[i]) == actconnect);

				// dark outline underneath the wire (soft shadow)
				HPEN oldp = (HPEN)SelectObject(hdc, DRAW.pen_wire_shadow);
				wire_path(hdc, x0, y0, x1, y1);

				// coloured wire on top
				SelectObject(hdc, selected ? DRAW.pen_wire_sel : DRAW.pen_red);
				wire_path(hdc, x0, y0, x1, y1);
				SelectObject(hdc, oldp);

				// small junction dots at both ends
				HBRUSH ob = (HBRUSH)SelectObject(hdc, selected ? DRAW.brush_blue : DRAW.brush_orange);
				HPEN pp = (HPEN)SelectObject(hdc, DRAW.pen_portedge);
				Ellipse(hdc, x1-3, y1-3, x1+3, y1+3);
				SelectObject(hdc, ob);
				SelectObject(hdc, pp);
			}
		}
	}
	SetBkMode(hdc, oldjoin);
}

void draw_captions(HDC hdc, WORD t)
{
	int i;
	char szdata[100];

		// --- module title text (white, bold, on the blue strip) ---
		SetTextColor (hdc, PALETTERGB(255, 255, 255));
		SetBkMode(hdc, TRANSPARENT);
		SelectObject(hdc, DRAW.titleFont);
		strcpy(szdata, objects[t]->tag);
		ExtTextOut(hdc, SX+objects[t]->xPos+5,SY+objects[t]->yPos+3, 0, NULL,szdata, strlen(szdata), NULL ) ;

		// --- port labels (dark text on the light body) ---
		SetTextColor(hdc, PALETTERGB(40, 40, 44));
		SelectObject(hdc, DRAW.scaleFont);

		// input terminals (green)
		SelectObject (hdc, DRAW.pen_portedge);
		SelectObject (hdc, DRAW.brush_yellow);
		for (i=0;i<objects[t]->inports;i++)
		{
			int px = SX+objects[t]->xPos;
			int py = SY+objects[t]->yPos+CON_START-4+i*CON_HEIGHT;
			switch(objects[t]->in_ports[i].in_type){
				case SFLOAT:
					RoundRect(hdc, px, py, px+CON_MAGNETIC, py+CON_MAGNETIC, 10, 10);
					break;
				case MFLOAT:
					Rectangle(hdc, px, py, px+CON_MAGNETIC, py+CON_MAGNETIC);
					break;
			}
		   if (!objects[t]->in_ports[i].in_name[0]) wsprintf(szdata,"%d",i+1); else strcpy(szdata,objects[t]->in_ports[i].in_name);
		   ExtTextOut(hdc, SX+objects[t]->xPos+12,SY+objects[t]->yPos-4+CON_START+i*CON_HEIGHT, 0, NULL,szdata, strlen(szdata), NULL ) ;

		}
		// output terminals (orange)
		SelectObject (hdc, DRAW.brush_orange);
		for (i=0;i<objects[t]->outports;i++)
		{
			int px = SX+objects[t]->xPos+objects[t]->width-CON_MAGNETIC;
			int py = SY+objects[t]->yPos+CON_START-4+i*CON_HEIGHT;
			switch(objects[t]->out_ports[i].out_type){
				case SFLOAT:
					RoundRect(hdc, px, py, px+CON_MAGNETIC, py+CON_MAGNETIC, 10, 10);
					break;
				case MFLOAT:
					Rectangle(hdc, px, py, px+CON_MAGNETIC, py+CON_MAGNETIC);
					break;
			}
		    if (!objects[t]->out_ports[i].out_name[0]) wsprintf(szdata,"%d",i+1); else strcpy(szdata,objects[t]->out_ports[i].out_name);
			SetTextAlign(hdc,TA_RIGHT);
		    ExtTextOut(hdc, SX+objects[t]->xPos+objects[t]->width-CON_MAGNETIC-4, SY+objects[t]->yPos-4+CON_START+i*CON_HEIGHT, 0, NULL,szdata, strlen(szdata), NULL ) ;
			SetTextAlign(hdc,TA_LEFT);
		}
}

void draw_objects(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC hdc;
	HDC memdc;
	HBITMAP membmp, oldbmp;
	RECT rcClient;

	WORD t;
	int a=-1;

	SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
    GetScrollInfo(ghWndDesign, SB_HORZ, &si);
    SX=-si.nPos;
    GetScrollInfo(ghWndDesign, SB_VERT, &si);
    SY=-si.nPos;

	hdc = BeginPaint (hWnd, &ps);
	GetClientRect(hWnd, &rcClient);

	// --- double buffer to avoid grid/wire flicker ---
	memdc  = CreateCompatibleDC(hdc);
	membmp = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
	oldbmp = (HBITMAP)SelectObject(memdc, membmp);

	// canvas background + grid
	FillRect(memdc, &rcClient, DRAW.brush_canvas);
	draw_grid(memdc, &rcClient);

	// wires first (so modules sit on top of their terminals cleanly),
	// then modules, then captions
	for (t=0;t<GLOBAL.objects;t++)	draw_connections(memdc,t);
	for (t=0;t<GLOBAL.objects;t++)
	{
		draw_object(memdc,t);
		draw_captions(memdc,t);
		if (objects[t]==actobject) a=t;
	}
	// redraw the active object on top with its highlighted wire
	if (a!=-1) { draw_object(memdc,a); draw_captions(memdc,a); draw_connections(memdc,a); }

	BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memdc, 0, 0, SRCCOPY);

	SelectObject(memdc, oldbmp);
	DeleteObject(membmp);
	DeleteDC(memdc);

	EndPaint( hWnd, &ps );

}




SDL_Surface *LoadBMP(char *filename)
{
	Uint8 *rowhi, *rowlo;
	Uint8 *tmpbuf, tmpch;
	SDL_Surface *image;
	int i, j;

	image = SDL_LoadBMP(filename);
	if ( image == NULL ) {
		fprintf(stderr, "Unable to load %s: %s\n", filename, SDL_GetError());
		return(NULL);
	}

	/* GL surfaces are upsidedown and RGB, not BGR :-) */
	tmpbuf = (Uint8 *)malloc(image->pitch);
	if ( tmpbuf == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	rowhi = (Uint8 *)image->pixels;
	rowlo = rowhi + (image->h * image->pitch) - image->pitch;
	for ( i=0; i<image->h/2; ++i ) {
		for ( j=0; j<image->w; ++j ) {
			tmpch = rowhi[j*3];
			rowhi[j*3] = rowhi[j*3+2];
			rowhi[j*3+2] = tmpch;
			tmpch = rowlo[j*3];
			rowlo[j*3] = rowlo[j*3+2];
			rowlo[j*3+2] = tmpch;
		}
		memcpy(tmpbuf, rowhi, image->pitch);
		memcpy(rowhi, rowlo, image->pitch);
		memcpy(rowlo, tmpbuf, image->pitch);
		rowhi += image->pitch;
		rowlo -= image->pitch;
	}
	free(tmpbuf);
	return(image);
}

int LoadGLTextures()									// Load Bitmap And Convert To A Texture
{
	char particlefilename[200];
    int Status=FALSE;								// Status Indicator
    SDL_Surface *TextureImage[1];				// Create Storage Space For The Textures
  

	memset(TextureImage,0,sizeof(void *)*1);		// Set The Pointer To NULL
	strcpy(particlefilename,GLOBAL.resourcepath);
	strcat(particlefilename,"GRAPHICS\\");
	strcat(particlefilename,PARTICLEBITMAP);
    if (TextureImage[0]=LoadBMP(particlefilename))	// Load Particle Texture
    {
		Status=TRUE;								// Set The Status To TRUE
		glGenTextures(1, &texture[0]);				// Create One Texture
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, TextureImage[0]->w, TextureImage[0]->h, 0, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[0]->pixels);
    }

    if (TextureImage[0])							// If Texture Exists
	{
		SDL_FreeSurface(TextureImage[0]);
	}
    return Status;		
}


//
// Called when a GL window is resized. Resizes the OpenGL
// viewport.
//
void Size_GL(HWND hWnd, HGLRC m_hRC, int asp)
{
			
GLfloat fFovy  = 30.0f; // Field-of-view
GLfloat fZNear = 0.1f;  // Near clipping plane
GLfloat fZFar  = 10000.0f;  // Far clipping plane
RECT rv;
GLfloat fAspect;
HDC hDC;

	if(hWnd==NULL) return;

	hDC = GetDC(hWnd);
	wglMakeCurrent(hDC, m_hRC);
	
	// Calculate viewport aspect
	GetClientRect(hWnd, &rv);
	fAspect = (GLfloat)(rv.right-rv.left) / (GLfloat)(rv.bottom-rv.top);
	if (asp==1) fAspect = 1.0;

	// Define viewport
	glViewport(rv.left, rv.top, rv.right-rv.left, rv.bottom-rv.top);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fFovy, fAspect, fZNear, fZFar);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();									// Reset The Modelview Matrix

	wglMakeCurrent(0, 0);
	ReleaseDC(hWnd, hDC);
}


//
// Shutdown_GL()
// Called when a GL window is destroyed. Shuts down OpenGL
//
void Shutdown_GL(HGLRC m_hRC)
{
	wglDeleteContext(m_hRC);
}



GLvoid KillFont(GLvoid)									// Delete The Font List
{
	glDeleteLists(base, 96);							// Delete All 96 Characters
}

GLvoid glPrint(const char *fmt, ...)					// Custom GL "Print" Routine
{
	char		text[256];								// Holds Our String
	va_list		ap;										// Pointer To List Of Arguments

	if (fmt == NULL)									// If There's No Text
		return;											// Do Nothing

	va_start(ap, fmt);									// Parses The String For Variables
	    vsprintf(text, fmt, ap);						// And Converts Symbols To Actual Numbers
	va_end(ap);											// Results Are Stored In Text

	glPushAttrib(GL_LIST_BIT);							// Pushes The Display List Bits
	glListBase(base - 32);								// Sets The Base Character to 32
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);	// Draws The Display List Text
	glPopAttrib();										// Pops The Display List Bits
}


 

