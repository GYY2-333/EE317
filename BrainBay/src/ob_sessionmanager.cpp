/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org
  
  MODULE: OB_SESSIONMANAGER.CPP:  contains functions for the Sessionmanager-Object

  The Sessionmanager-Object has its own window, it uses GDI-drawings 

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#include "brainBay.h"
#include "ob_sessionmanager.h"
#include <wingdi.h> 

#define NAVI_WIDTH 200
#define NAVI_HEIGHT 200
#define NAVI_SELECTDISTANCE 40

int find_bmpfiles(char * reportname, int select, char * targetfilename) {
	char reportpath[256];
	strcpy(reportpath,GLOBAL.resourcepath); 
	strcat(reportpath,"REPORTS\\");
	strcat(reportpath,reportname);
	strcat(reportpath,"*.bmp");
	// printf("folder: %s\n",reportpath);
	if (!reportname) return 0;
	if (!strlen(reportname)) return 0;

	HANDLE hFind;
	WIN32_FIND_DATA data;
	int count =0;

	hFind = FindFirstFile(reportpath, &data);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// printf("found: %s\n", data.cFileName);
			if (count==select)
				strcpy (targetfilename, data.cFileName);
			count++;
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return(count);
}

void parse_menuitems(SESSIONMANAGEROBJ* st)
{
	int pos=0;
	char szdata[256];
	char * tmp=st->sessionlist;

	st->menuitems=0;
	while (*tmp) {
		pos=0;

		// read a whole line
		while ((*tmp!=0) && (*tmp!='\r') && (*tmp!='\n'))
		{
			szdata[pos++]=*tmp;
			tmp++;
		}
		while ((*tmp!=0) && ((*tmp=='\r') || (*tmp=='\n'))) tmp++;
		szdata[pos]=0;

		// get sessionname
		char *tmp2=szdata;
		int pos2=0;
		while ((*tmp2) && (*tmp2!='#')) {
		   st->sessionname[st->menuitems][pos2++]=*tmp2;
		   tmp2++;
		}
		st->sessionname[st->menuitems][pos2]=0;

		pos2=0;
		// get sessionpath
		if (*tmp2) {
			tmp2++; 
			while ((*tmp2) && (*tmp2!='#')) {
			   st->sessionpath[st->menuitems][pos2++]=*tmp2;
			   tmp2++;
			}
		}
		st->sessionpath[st->menuitems][pos2]=0;

		pos2=0;
		// get sessionreport
		if (*tmp2) {
			tmp2++;
			while ((*tmp2) && (*tmp2!='#')) {
			   st->sessionreport[st->menuitems][pos2++]=*tmp2;
			   tmp2++;
			}
		}
		st->sessionreport[st->menuitems][pos2]=0;

		// get all bitmap files for the sessionreport
		st->maxreportitems[st->menuitems] = find_bmpfiles(st->sessionreport[st->menuitems], -1, NULL);
		st->menuitems++;
	}
}


void draw_sessionmanager(SESSIONMANAGEROBJ * st)
{
	PAINTSTRUCT ps;
	HDC hdc;
	char szdata[256];
	RECT rect, rc;

	// ── LabVIEW-style modern dark palette ──────────────────────────────────
	const COLORREF CLR_BG        = RGB( 28,  28,  30); // near-black background
	const COLORREF CLR_PANEL     = RGB( 44,  44,  48); // slightly lighter panel
	const COLORREF CLR_ACCENT    = RGB( 30,  87, 149); // LabVIEW blue accent
	const COLORREF CLR_ACCENT_HI = RGB( 50, 120, 200); // lighter blue for hover
	const COLORREF CLR_SEP       = RGB( 60,  60,  65); // separator line
	const COLORREF CLR_TXT_HI    = RGB(255, 255, 255); // selected item text
	const COLORREF CLR_TXT       = RGB(180, 180, 190); // normal item text
	const COLORREF CLR_SUBTITLE  = RGB(120, 160, 210); // subtitle text
	const COLORREF CLR_BULLET    = RGB( 30,  87, 149); // bullet dot
	// ───────────────────────────────────────────────────────────────────────

	GetClientRect(st->displayWnd, &rect);
	hdc = BeginPaint(st->displayWnd, &ps);

	// ── 1. Background fill ──────────────────────────────────────────────────
	HBRUSH hBrBg = CreateSolidBrush(CLR_BG);
	FillRect(hdc, &rect, hBrBg);
	DeleteObject(hBrBg);

	int W = rect.right;
	int H = rect.bottom;

	// ── 2. Left accent sidebar ─────────────────────────────────────────────
	RECT rcSidebar = { 0, 0, 6, H };
	HBRUSH hBrAccent = CreateSolidBrush(CLR_ACCENT);
	FillRect(hdc, &rcSidebar, hBrAccent);
	DeleteObject(hBrAccent);

	// ── 3. Top header panel ────────────────────────────────────────────────
	int headerH = max(70, H / 8);
	RECT rcHeader = { 6, 0, W, headerH };
	HBRUSH hBrPanel = CreateSolidBrush(CLR_PANEL);
	FillRect(hdc, &rcHeader, hBrPanel);
	DeleteObject(hBrPanel);

	// header bottom separator line
	HPEN hPenSep = CreatePen(PS_SOLID, 2, CLR_ACCENT);
	HPEN hPenOld = (HPEN)SelectObject(hdc, hPenSep);
	MoveToEx(hdc, 6, headerH, NULL);
	LineTo(hdc, W, headerH);
	SelectObject(hdc, hPenOld);
	DeleteObject(hPenSep);

	// ── 4. Header title text ───────────────────────────────────────────────
	SetBkMode(hdc, TRANSPARENT);

	int titleSize = max(16, headerH / 3);
	HFONT hFontTitle = CreateFont(
		-MulDiv(titleSize, GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	HFONT hFontOld = (HFONT)SelectObject(hdc, hFontTitle);

	char titleText[80];
	strncpy(titleText, st->wndcaption, 79);
	// strip window-caption suffix after " - " if present
	char* dash = strstr(titleText, " - ");
	if (dash) *dash = 0;

	SetTextColor(hdc, CLR_TXT_HI);
	// title occupies the top half of the header
	rc = { 20, 4, W - 20, headerH / 2 };
	DrawText(hdc, titleText, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	// subtitle occupies the bottom half of the header
	int subSize = max(9, titleSize * 2 / 3);
	HFONT hFontSub = CreateFont(
		-MulDiv(subSize, GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	SelectObject(hdc, hFontSub);
	SetTextColor(hdc, CLR_SUBTITLE);
	rc = { 22, headerH / 2, W - 20, headerH - 4 };
	DrawText(hdc, "Select a session and press Enter or click to start", -1, &rc,
		DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	DeleteObject(hFontSub);

	// ── 5. Menu items as modern cards ─────────────────────────────────────
	int menuFontSize = max(12, st->fontsize);
	HFONT hFontMenu = CreateFont(
		-MulDiv(menuFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	SelectObject(hdc, hFontMenu);

	int cardMarginL = max(24, W / 12);
	int cardW       = min(480, W * 55 / 100);
	int cardH       = max(38, menuFontSize * 3);
	int cardGap     = max(8,  cardH / 5);
	int startY      = headerH + max(20, H / 18);

	for (int i = 0; i < st->menuitems; i++)
	{
		int cy = startY + i * (cardH + cardGap);
		if (cy + cardH > H - 10) break;

		bool selected = (i == st->actmenuitem);

		// Card background
		RECT rcCard = { cardMarginL, cy, cardMarginL + cardW, cy + cardH };
		HBRUSH hBrCard = CreateSolidBrush(selected ? CLR_ACCENT : CLR_PANEL);
		FillRect(hdc, &rcCard, hBrCard);
		DeleteObject(hBrCard);

		// Card left accent strip
		RECT rcStrip = { cardMarginL, cy, cardMarginL + 4, cy + cardH };
		HBRUSH hBrStrip = CreateSolidBrush(selected ? CLR_ACCENT_HI : CLR_SEP);
		FillRect(hdc, &rcStrip, hBrStrip);
		DeleteObject(hBrStrip);

		// Card border (subtle)
		HPEN hPenCard = CreatePen(PS_SOLID, 1, selected ? CLR_ACCENT_HI : CLR_SEP);
		HPEN hPenC = (HPEN)SelectObject(hdc, hPenCard);
		SelectObject(hdc, GetStockObject(NULL_BRUSH));
		Rectangle(hdc, rcCard.left, rcCard.top, rcCard.right, rcCard.bottom);
		SelectObject(hdc, hPenC);
		DeleteObject(hPenCard);

		// Item text
		char actname[120];
		if (st->maxreportitems[i])
			wsprintf(actname, "%s  (%d)", st->sessionname[i], st->maxreportitems[i]);
		else
			wsprintf(actname, "%s", st->sessionname[i]);

		SetTextColor(hdc, selected ? CLR_TXT_HI : CLR_TXT);
		SetBkMode(hdc, TRANSPARENT);
		RECT rcText = { cardMarginL + 14, cy, cardMarginL + cardW - 10, cy + cardH };
		DrawText(hdc, actname, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

		// Arrow indicator on selected
		if (selected)
		{
			HPEN hPenArr = CreatePen(PS_SOLID, 2, CLR_TXT_HI);
			SelectObject(hdc, hPenArr);
			int ax = cardMarginL + cardW - 18;
			int ay = cy + cardH / 2;
			MoveToEx(hdc, ax,     ay - 6, NULL); LineTo(hdc, ax + 6, ay);
			MoveToEx(hdc, ax,     ay + 6, NULL); LineTo(hdc, ax + 6, ay);
			DeleteObject(SelectObject(hdc, hPenC));
		}
	}

	// ── 6. Navigation hint (bottom-right) ─────────────────────────────────
	int hintSize = max(8, W / 120);
	HFONT hFontHint = CreateFont(
		-MulDiv(hintSize, GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	SelectObject(hdc, hFontHint);
	SetTextColor(hdc, CLR_SEP);
	rc = { W / 2, H - 28, W - 16, H - 8 };
	DrawText(hdc, "Arrow keys or click to navigate  |  Enter to start", -1, &rc,
		DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
	DeleteObject(hFontHint);

	// ── 7. Bitmap/report rendering (unchanged logic, repositioned) ─────────
	if (strlen(st->actreport) > 0)
	{
		char reportpath[256];
		strcpy(reportpath, GLOBAL.resourcepath);
		strcat(reportpath, "REPORTS\\");
		strcat(reportpath, st->actreport);

		HBITMAP hBitmap = (HBITMAP)LoadImage(hInst, reportpath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		if (hBitmap)
		{
			HDC hdcMem = CreateCompatibleDC(hdc);
			HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);
			BITMAP  bitmap;
			GetObject(hBitmap, sizeof(bitmap), &bitmap);

			int previewX = cardMarginL + cardW + 20;
			int previewW = W - previewX - 16;
			if (previewW > 60)
			{
				float g = (float)previewW / (float)bitmap.bmWidth;
				int   pH = (int)((float)bitmap.bmHeight * g);
				int   pY = headerH + (H - headerH - pH) / 2;
				if (pY < headerH + 10) pY = headerH + 10;
				StretchBlt(hdc, previewX, pY, previewW, pH,
					hdcMem, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
			}
			SelectObject(hdcMem, oldBitmap);
			DeleteDC(hdcMem);
			DeleteObject(hBitmap);
		}
	}

	// cleanup fonts
	SelectObject(hdc, hFontOld);
	DeleteObject(hFontTitle);
	DeleteObject(hFontMenu);

	st->redraw = 0;
	EndPaint(st->displayWnd, &ps);
}

int distance(int x1,int y1, int x2, int y2)
{
	return(sqrt ((float)((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2))));
}


LRESULT CALLBACK SessionmanagerDlgHandler( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static int init;
	char temp[100];
	SESSIONMANAGEROBJ * st;
	int x;
	
	st = (SESSIONMANAGEROBJ *) actobject;
    if ((st==NULL)||(st->type!=OB_SESSIONMANAGER)) return(FALSE);
					
	switch( message )
	{
		case WM_INITDIALOG:
			{
				SCROLLINFO lpsi;
				lpsi.cbSize=sizeof(SCROLLINFO);
				lpsi.fMask=SIF_RANGE; // |SIF_POS;
				lpsi.nMin=0; lpsi.nMax=100;
				SetScrollInfo(GetDlgItem(hDlg,IDC_FONTSIZEBAR),SB_CTL,&lpsi,TRUE);
				SetScrollPos(GetDlgItem(hDlg,IDC_FONTSIZEBAR), SB_CTL,st->fontsize,TRUE);
				SetDlgItemInt(hDlg, IDC_FONTSIZE, st->fontsize,0);

				lpsi.nMin=0; lpsi.nMax=500;
				SetScrollInfo(GetDlgItem(hDlg,IDC_BITMAPSIZEBAR),SB_CTL,&lpsi,TRUE);
				SetScrollPos(GetDlgItem(hDlg,IDC_BITMAPSIZEBAR), SB_CTL,st->fontsize,TRUE);
				SetDlgItemInt(hDlg, IDC_BITMAPSIZE, st->bitmapsize,0);

				SetDlgItemText(hDlg, IDC_WINDOWCAPTION, st->wndcaption);
				SetDlgItemText(hDlg, IDC_LOGOPATH, st->logopath);
				SetDlgItemText(hDlg, IDC_SESSIONLIST, st->sessionlist);
				CheckDlgButton(hDlg, IDC_DISPLAYNAVIGATION, st->displaynavigation);

				SetDlgItemInt(hDlg, IDC_MENU_X, st->menu_x,0);
				SetDlgItemInt(hDlg, IDC_MENU_Y, st->menu_y,0);
				SetDlgItemInt(hDlg, IDC_NAVCROSS_X, st->navcross_x,0);
				SetDlgItemInt(hDlg, IDC_NAVCROSS_Y, st->navcross_y,0);
				SetDlgItemInt(hDlg, IDC_LOGO_X, st->logo_x,0);
				SetDlgItemInt(hDlg, IDC_LOGO_Y, st->logo_y,0);

			}
			return TRUE;
	
		case WM_CLOSE:
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
			case IDC_SELECTCOLOR:
				st->selectcolor=select_color(hDlg,st->selectcolor);
				st->redraw=1;
				InvalidateRect(hDlg,NULL,FALSE);
				InvalidateRect(st->displayWnd,NULL,FALSE);
				break;
			case IDC_FONTCOL:
				st->fontcolor=select_color(hDlg,st->fontcolor);
				st->redraw=1;
				InvalidateRect(hDlg,NULL,FALSE);
				InvalidateRect(st->displayWnd,NULL,FALSE);
				break;
			case IDC_FONTBKCOL:
				st->fontbkcolor=select_color(hDlg,st->fontbkcolor);
				st->redraw=1;
				InvalidateRect(hDlg,NULL,FALSE);
				InvalidateRect(st->displayWnd,NULL,FALSE);
				break;
			case IDC_BKCOLOR:
				st->bkcolor=select_color(hDlg,st->bkcolor);
				st->redraw=1;
				InvalidateRect(hDlg,NULL,FALSE);
				InvalidateRect(st->displayWnd,NULL,FALSE);
				break;
			case IDC_SESSIONLIST:
				GetDlgItemText(hDlg,IDC_SESSIONLIST,st->sessionlist,4096); 
				parse_menuitems(st);
				st->redraw=1;
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_WINDOWCAPTION:
				GetDlgItemText(hDlg,IDC_WINDOWCAPTION,st->wndcaption,80); 
				SetWindowText(st->displayWnd,st->wndcaption);
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_LOGOPATH:
				GetDlgItemText(hDlg,IDC_LOGOPATH,st->logopath,100); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_DISPLAYNAVIGATION:
				 st->displaynavigation=  IsDlgButtonChecked(hDlg,IDC_DISPLAYNAVIGATION);
  				 st->redraw=1;
  				 InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_MENU_X:
				st->menu_x=GetDlgItemInt(hDlg,IDC_MENU_X,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_MENU_Y:
				st->menu_y=GetDlgItemInt(hDlg,IDC_MENU_Y,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_NAVCROSS_X:
				st->navcross_x=GetDlgItemInt(hDlg,IDC_NAVCROSS_X,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_NAVCROSS_Y:
				st->navcross_y=GetDlgItemInt(hDlg,IDC_NAVCROSS_Y,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_LOGO_X:
				st->logo_x=GetDlgItemInt(hDlg,IDC_LOGO_X,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			case IDC_LOGO_Y:
				st->logo_y=GetDlgItemInt(hDlg,IDC_LOGO_Y,NULL,0); 
				InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
			}
			return TRUE;

		case WM_HSCROLL:
		{
			int nNewPos;
			nNewPos=get_scrollpos(wParam,lParam);
			if (lParam == (long) GetDlgItem(hDlg,IDC_FONTSIZEBAR))  {
				SetDlgItemInt(hDlg, IDC_FONTSIZE,nNewPos,TRUE);
				st->fontsize=nNewPos;
				if (st->font) DeleteObject(st->font);
				st->font = CreateFont(-MulDiv(st->fontsize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
			}
			if (lParam == (long) GetDlgItem(hDlg,IDC_BITMAPSIZEBAR))  {
				SetDlgItemInt(hDlg, IDC_BITMAPSIZE,nNewPos,TRUE);
				st->bitmapsize=nNewPos;
			}
			if (st->displayWnd) {st->redraw=1; InvalidateRect(st->displayWnd,NULL,TRUE);}		
		}	break;

		case WM_SIZE:
		case WM_MOVE:  update_toolbox_position(hDlg);
		case WM_PAINT:
			color_button(GetDlgItem(hDlg,IDC_SELECTCOLOR),st->selectcolor);
			color_button(GetDlgItem(hDlg,IDC_FONTCOL),st->fontcolor);
			color_button(GetDlgItem(hDlg,IDC_BKCOLOR),st->bkcolor);
			color_button(GetDlgItem(hDlg,IDC_FONTBKCOL),st->fontbkcolor);
			break;
	}
    return FALSE;
}


LRESULT CALLBACK SessionManagerWndHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{   
	int t;
	SESSIONMANAGEROBJ * st;
	st=NULL;
	for (t=0;(t<GLOBAL.objects)&&(st==NULL);t++)
		if (objects[t]->type==OB_SESSIONMANAGER)
		{	st=(SESSIONMANAGEROBJ *)objects[t];
		    if (st->displayWnd!=hWnd) st=NULL;
		}

	if (st==NULL) return DefWindowProc( hWnd, message, wParam, lParam );
	
	switch( message ) 
	{	case WM_DESTROY:
		 break;
		case WM_KEYDOWN:
			// printf("key: %d",wParam);
		    switch(wParam) {
				case KEY_UP:
				  if (st->actmenuitem>0) {
					  st->actmenuitem--;
					  if (st->maxreportitems[st->actmenuitem]) {
							st->actreportitem=st->maxreportitems[st->actmenuitem]-1;
							find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
					  }
					  else { st->actreportitem=0; st->actreport[0]=0;}
				  }
	   			  InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
				case KEY_DOWN:
				  if (st->actmenuitem<st->menuitems-1) {
					  st->actmenuitem++;
					  if (st->maxreportitems[st->actmenuitem]) {
							st->actreportitem=st->maxreportitems[st->actmenuitem]-1;
							find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
					  }
					  else { st->actreportitem=0; st->actreport[0]=0;}
				  }
	   			  InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
				case KEY_LEFT:
					if (st->maxreportitems[st->actmenuitem]) {
							if (st->actreportitem>0) st->actreportitem--;
							find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
					}
	   			  InvalidateRect(st->displayWnd,NULL,TRUE);
				break;

				case KEY_RIGHT:
					if (st->maxreportitems[st->actmenuitem]) {
							if (st->actreportitem<st->maxreportitems[st->actmenuitem]-1) 
								st->actreportitem++;
							find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
					}
	   			  InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
				case KEY_BACKSPACE:
 				  if (st->actmenuitem>0) {
					    find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
  						char tmptxt[500];
						wsprintf(tmptxt,"Do you really want to remove the report graph %s",st->actreport);
						if (MessageBox(NULL, tmptxt, "Confirm removal", MB_YESNO) == IDYES) 
						{
							char reportpath[256];
							strcpy(reportpath,GLOBAL.resourcepath); 
							strcat(reportpath,"REPORTS\\");
							strcat(reportpath,st->actreport);
							printf ("remove bitmap: %s !!\n",reportpath);
							DeleteFile(reportpath);
						}
						parse_menuitems(st);
						if (st->maxreportitems[st->actmenuitem]) {
							st->actreportitem=st->maxreportitems[st->actmenuitem]-1;
							find_bmpfiles(st->sessionreport[st->actmenuitem], st->actreportitem, st->actreport);
						}
						else { st->actreportitem=0; st->actreport[0]=0;}
						st->redraw=1;
						InvalidateRect(st->displayWnd,NULL,FALSE);
				  }
				break;


				case KEY_ENTER:
				  if (st->actmenuitem<st->menuitems) 
				  {
						char configfilename[MAX_PATH];
						close_toolbox();
						strcpy(configfilename,GLOBAL.resourcepath); 
						strcat(configfilename,"CONFIGURATIONS\\");
						strcat(configfilename,st->sessionpath[st->actmenuitem]);
						strcat(configfilename,".con");
						// printf("trying to load configfile: %s\n",configfilename);
						if (!load_configfile(configfilename)) 
							report_error("Could not load Config File");
						else sort_objects();					  
				  }
	   			  InvalidateRect(st->displayWnd,NULL,TRUE);
				break;
				}
			break;
		case WM_MOUSEACTIVATE:
   	      st->redraw=1;
		  close_toolbox();
		  actobject=st;
		  SetWindowPos(hWnd,HWND_TOP,0,0,0,0,SWP_DRAWFRAME|SWP_NOMOVE|SWP_NOSIZE);
		  InvalidateRect(ghWndDesign,NULL,TRUE);
			break;

		case WM_LBUTTONDOWN:
			{
			   int actx = (int)LOWORD(lParam);
			   int acty = (int)HIWORD(lParam);

			   // Hit-test against card layout (mirrors draw_sessionmanager geometry)
			   RECT rcClient;
			   GetClientRect(hWnd, &rcClient);
			   int W2 = rcClient.right;
			   int H2 = rcClient.bottom;

			   int headerH2   = max(70, H2 / 8);
			   int cardMarginL2 = max(24, W2 / 12);
			   int menuFontSize2 = max(12, st->fontsize);
			   int cardH2     = max(38, menuFontSize2 * 3);
			   int cardGap2   = max(8,  cardH2 / 5);
			   int cardW2     = min(480, W2 * 55 / 100);
			   int startY2    = headerH2 + max(20, H2 / 18);

			   for (int i = 0; i < st->menuitems; i++)
			   {
				   int cy = startY2 + i * (cardH2 + cardGap2);
				   if (cy + cardH2 > H2 - 10) break;
				   if (actx >= cardMarginL2 && actx <= cardMarginL2 + cardW2 &&
				       acty >= cy && acty <= cy + cardH2)
				   {
					   if (st->actmenuitem == i)
					   {
						   // click already-selected item → launch session
						   SendMessage(hWnd, WM_KEYDOWN, KEY_ENTER, 0);
					   }
					   else
					   {
						   st->actmenuitem = i;
						   if (st->maxreportitems[i]) {
							   st->actreportitem = st->maxreportitems[i] - 1;
							   find_bmpfiles(st->sessionreport[i], st->actreportitem, st->actreport);
						   } else { st->actreportitem = 0; st->actreport[0] = 0; }
						   st->redraw = 1;
						   InvalidateRect(hWnd, NULL, TRUE);
					   }
					   break;
				   }
			   }
			}
			break;

		case WM_SIZE: 
		case WM_MOVE:
			{
  			  WINDOWPLACEMENT  wndpl;
			  GetWindowPlacement(st->displayWnd, &wndpl);
  	 	      st->redraw=TRUE;

			  if (GLOBAL.locksession) {
				  wndpl.rcNormalPosition.top=st->top;
				  wndpl.rcNormalPosition.left=st->left;
				  wndpl.rcNormalPosition.right=st->right;
				  wndpl.rcNormalPosition.bottom=st->bottom;
				  SetWindowPlacement(st->displayWnd, &wndpl);
 				  SetWindowLong(st->displayWnd, GWL_STYLE, GetWindowLong(st->displayWnd, GWL_STYLE)&~WS_SIZEBOX);
			  }
			  else {
				  st->top=wndpl.rcNormalPosition.top;
				  st->left=wndpl.rcNormalPosition.left;
				  st->right=wndpl.rcNormalPosition.right;
				  st->bottom=wndpl.rcNormalPosition.bottom;
				  st->redraw=TRUE; 
				  st->redraw=TRUE;
				  SetWindowLong(st->displayWnd, GWL_STYLE, GetWindowLong(st->displayWnd, GWL_STYLE) | WS_SIZEBOX);
			  }
			  InvalidateRect(hWnd,NULL,TRUE);
			}
			break;

		case WM_ERASEBKGND:
			st->redraw=1;
			return 0;

		case WM_PAINT:
			draw_sessionmanager(st);
  	    	break;
		default:
			return DefWindowProc( hWnd, message, wParam, lParam );
    } 
    return 0;
}


//
//  Object Implementation
//


SESSIONMANAGEROBJ::SESSIONMANAGEROBJ(int num) : BASE_CL()
{
	outports = 0;
	inports = 0;
	width=115;
	height=50;
	// strcpy(in_ports[0].in_name,"in");
	// strcpy(out_ports[0].out_name,"out");

	strcpy (wndcaption,"Session Manager");
	strcpy (logopath,"");
	strcpy (sessionlist,"Testsession1#testconfig#testgraph1\r\nTestsession2#testconfig2#testgraph2");
	displaynavigation=1;
	redraw=1;
	fontsize=18;
	bitmapsize=100;
	left=10;right=550;top=20;bottom=400;
    menu_x=40; menu_y=40;
	navcross_x=750;navcross_y=140;
	logo_x=650,logo_y=50;

	actmenuitem=0;
	actreportitem=0;
	actreport[0]=0;
	parse_menuitems(this);

	selectcolor=RGB( 30,  87, 149);   // LabVIEW accent blue
	bkcolor    =RGB( 28,  28,  30);   // near-black background
	fontcolor  =RGB(180, 180, 190);   // light gray text
	fontbkcolor=RGB( 44,  44,  48);   // panel color

	if (!(font = CreateFont(-MulDiv(fontsize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial")))
		report_error("Font creation failed!");
	if (!(smallfont = CreateFont(-MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial")))
		report_error("Font creation failed!");

	if(!(displayWnd=CreateWindow("SessionManager_Class", wndcaption, WS_CLIPSIBLINGS | WS_CHILD, left, top, right-left, bottom-top, ghWndMain, NULL, hInst, NULL)))
		report_error("can't create SessionManager Window");
	else { SetForegroundWindow(displayWnd); ShowWindow( displayWnd, TRUE ); UpdateWindow( displayWnd ); }

	InvalidateRect(displayWnd, NULL, TRUE);
}

void SESSIONMANAGEROBJ::make_dialog(void)
{
	display_toolbox(hDlg=CreateDialog(hInst, (LPCTSTR)IDD_SESSIONMANAGERBOX, ghWndMain, (DLGPROC)SessionmanagerDlgHandler));
}
void SESSIONMANAGEROBJ::load(HANDLE hFile) 
{
	float temp;
	load_object_basics(this);

	load_property("selectcolor",P_FLOAT,&temp);
	selectcolor=(COLORREF)temp;
	load_property("bkcol",P_FLOAT,&temp);
	bkcolor=(COLORREF)temp;
	if (bkcolor==selectcolor) bkcolor=RGB(255,255,255);
	temp=0;bitmapsize=100;
	load_property("fontcol",P_FLOAT,&temp);
	fontcolor=(COLORREF)temp;
	temp=RGB(255,255,255);
	load_property("fontbkcol",P_FLOAT,&temp);
	fontbkcolor=(COLORREF)temp;
	
	load_property("top",P_INT,&top);
	load_property("left",P_INT,&left);
	load_property("right",P_INT,&right);
	load_property("bottom",P_INT,&bottom);
	load_property("fontsize",P_INT,&fontsize);
	if (fontsize)
	{
		if (font) DeleteObject(font);
		if (!(font = CreateFont(-MulDiv(fontsize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial")))
			report_error("Font creation failed!");
	} 
	load_property("wndcaption",P_STRING,wndcaption);
	load_property("logopath",P_STRING,logopath);
	load_property("sessionlist",P_STRING,sessionlist);
	char * tmp=sessionlist;
	while (*tmp) { if (*tmp=='?') *tmp='\n'; if (*tmp=='~') *tmp='\r'; tmp++; } 
	load_property("displaynavigation",P_INT,&displaynavigation);
	load_property("bitmapsize",P_INT,&bitmapsize);
	load_property("menu_x",P_INT,&menu_x);
	load_property("menu_y",P_INT,&menu_y);
	load_property("navcross_x",P_INT,&navcross_x);
	load_property("navcross_y",P_INT,&navcross_y);
	load_property("logo_x",P_INT,&logo_x);
	load_property("logo_y",P_INT,&logo_y);

	parse_menuitems(this);
	if (maxreportitems[0]>0)
		actreportitem=maxreportitems[0]-1;
	else actreportitem=0;

	MoveWindow(displayWnd,left,top,right-left,bottom-top,TRUE); 
	if (GLOBAL.locksession) {
 		SetWindowLong(displayWnd, GWL_STYLE, GetWindowLong(displayWnd, GWL_STYLE)&~WS_SIZEBOX);
		//SetWindowLong(displayWnd, GWL_STYLE, 0);
	} else { SetWindowLong(displayWnd, GWL_STYLE, GetWindowLong(displayWnd, GWL_STYLE) | WS_SIZEBOX); }
	InvalidateRect (displayWnd, NULL, TRUE);

	SetWindowText(displayWnd,wndcaption);
	redraw=1;
}
		
void SESSIONMANAGEROBJ::save(HANDLE hFile) 
{	  
	float temp;
	save_object_basics(hFile, this);
	
	temp=(float)selectcolor;
	save_property(hFile,"selectcolor",P_FLOAT,&temp);
	temp=(float)bkcolor;
	save_property(hFile,"bkcol",P_FLOAT,&temp);
	temp=(float)fontcolor;
	save_property(hFile,"fontcol",P_FLOAT,&temp);
	temp=(float)fontbkcolor;
	save_property(hFile,"fontbkcol",P_FLOAT,&temp);
	
	save_property(hFile,"top",P_INT,&top);
	save_property(hFile,"left",P_INT,&left);
	save_property(hFile,"right",P_INT,&right);
	save_property(hFile,"bottom",P_INT,&bottom);
	save_property(hFile,"fontsize",P_INT,&fontsize);
	save_property(hFile,"wndcaption",P_STRING,wndcaption);
	save_property(hFile,"logopath",P_STRING,logopath);

	char * tmp=sessionlist;
	while (*tmp) { if (*tmp=='\n') *tmp='?'; if (*tmp=='\r') *tmp='~'; tmp++; } 
	save_property(hFile,"sessionlist",P_STRING,sessionlist);
	tmp=sessionlist;
	while (*tmp) { if (*tmp=='?') *tmp='\n'; if (*tmp=='~') *tmp='\r'; tmp++; } 

	save_property(hFile,"displaynavigation",P_INT,&displaynavigation);
	save_property(hFile,"bitmapsize",P_INT,&bitmapsize);
	save_property(hFile,"menu_x",P_INT,&menu_x);
	save_property(hFile,"menu_y",P_INT,&menu_y);
	save_property(hFile,"navcross_x",P_INT,&navcross_x);
	save_property(hFile,"navcross_y",P_INT,&navcross_y);
	save_property(hFile,"logo_x",P_INT,&logo_x);
	save_property(hFile,"logo_y",P_INT,&logo_y);
}

void SESSIONMANAGEROBJ::incoming_data(int port, float value)
{
}
        
void SESSIONMANAGEROBJ::work(void) 
{
	if ((displayWnd)&&(!TIMING.draw_update)&&(!GLOBAL.fly)) 
		InvalidateRect(displayWnd,NULL,FALSE);	  
}
	  
SESSIONMANAGEROBJ::~SESSIONMANAGEROBJ()
{
	if  (displayWnd!=NULL){ DestroyWindow(displayWnd); displayWnd=NULL; }
}  
