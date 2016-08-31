// zasmview.cpp : Defines the entry point for the DLL application.
//

#include <wchar.h>
#include <stdio.h>
#include "stdafx.h"
#include "zasmview.h"
#include "cunicode.h"

#define BUFF_SIZE 131072

#define supportedextension1 L".$a"
#define supportedextension2 L".$asm"
#define supportedextension3 L".!a"
#define supportedextension4 L".!asm"
#define supportedextension5 L".asm"
#define supportedextension6 L".a"
#define supportedextension7 L".zas"
#define supportedextension8 L".$z"
#define supportedextension9 L".!z"
/* Note: in C, double quotes inside a string need to be escaped with a backslash!  */
#define parsefunction "ext=\"$A\" | ext=\"!A\" | ext=\"A\" | ext=\"Z\" | ext=\"$Z\" | ext=\"!Z\" | ext=\"ZAS\" | ext=\"$C\" | ext=\"!C\" | ext=\"ASM\" | ext=\"C\""

HINSTANCE hinst;
HMODULE FLibHandle=0;
HFONT       g_hf      = NULL; // Font

char tokens[][8]={"ld","ex","im","rst","ret","add","adc","sub","sbc","and","xor","or","cp","push","pop","inc","dec","in","out","jp",
				"call","jr","djnz","rlc","rrc","rl","rr","sla","sra","sli","srl","bit","res","set","nop","halt","di","ei","rlca","rla",
				"rrca","rra","exx","daa","cpl","ccf","scf","ldi","ldir","ldd","lddr","cpi","cpir","cpd","cpdr","neg","inf","ini","inir",
				"ind","indr","outi","otir","outd","otdr","reti","retn","rld","rrd","org","equ","db","dw","ds","defb","defw","defs","insert",
				"include","if","ifdef","ifndef","ifused","ifnused","else","endif","make","b","c","d","e","h","l","(hl)","a","xh","xl",
				"yh","yl","(ix","(iy","(bc)","(de)","i","r","af","bc","de","hl","ix","iy","sp","(sp)","af'","(c)","nz","z","nc","c","po",
				"pe","p","m","phase","unphase","dc","ent","rept","endr","loadtab","macro","endm","create","makelab","saveobj","exitm",
				"ifp","exa","retz","retnz","retc","retnc","retm","retp","retpo","retpe","jpz","jpnz","jpc","jpnc","jpm","jpp","jppo","jppe",
				"callz","callc","callm","callpe","callnz","callnc","callp","callpo","jrz","jrnz","jrc","jrnc","project","eq"};


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			hinst=(HINSTANCE)hModule;
			break;
		case DLL_PROCESS_DETACH:
			if (FLibHandle)
				FreeLibrary(FLibHandle);
			FLibHandle=NULL;
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
    }
    return TRUE;
}

char* strlcpy(char* p,char*p2,int maxlen)
{
	if ((int)strlen(p2)>=maxlen) {
		strncpy(p,p2,maxlen);
		p[maxlen]=0;
	} else
		strcpy(p,p2);
	return p;
}

bool CheckFormat(char* inbuf,int buflen)
{
	char* p;
	p=inbuf;
	int success = 0, caret=0,tkns;
	unsigned char ch = 0,skip = 0;
	//Проверяем первые 2кб файла
	int len = (buflen>2048)?2048:buflen;
	while (len--)
	{
		ch = p[0];
		p++;
		if (ch==0) {
				success = 0;
				break;
		}
		if (skip) {
			skip = 0;
			continue;
		}
		if (ch==6) {
			if ((unsigned char)p[0]>129) {
				//счетчик SPC-компрессора должен быть больше 1
				success++;
				skip = 1;
				continue;
			} else {
				success = 0;
				break;
			}
		}
		if (ch>=2 && ch<=5) {
			//Токен?
			if ((unsigned char)p[0]>=32 && (unsigned char)p[0]<=0xc8)
			{
				success++;
				tkns++;
				skip = 1;
			} else {
				//Не токен!!!
				success = 0;
				break;
			}
			continue;
		}
		if (ch==13)
		{
			//После 13 не должно быть 10
			if ((unsigned char)p[0]==10) {
				//Это скорее всего обычный текст!
				success = 0;
				break;
			}
			caret++;
			success++;
			continue;
		}
	}
	if (success>0) {
		len = (buflen>2048)?2048:buflen;
		if (tkns==0 && (caret==0 || caret<len/255)) success = 0;
	}
	return (success==0)?false:true;
}

char* Convert(char* inbuf,int buflen, bool InsertNumbers)
{
	int j=0;
	char* p;
	static char strerr[256];
	char* pout=(char*)malloc(BUFF_SIZE);
	char* pout1;
	if (!pout) {
		return NULL;
	}
	int currentline=1;
	p=inbuf;
	char* p2 = pout;
	char tkn = 0;	//Предыдущий байт был токен-командой
	unsigned char ch = 0;
	char spc_comp = 0;
	p2[0]=0;
	if (InsertNumbers) {
		wsprintf(strerr,"%d:\t",currentline);
		strcat(p2,strerr);
		p2+=strlen(strerr);
	}
	while (buflen--) {
				ch = p[0];
				p++;
				if (ch==0) {
					//конец файла
					p[2] = 0x00;
					buflen = 0;	
					break;
				}
				if (spc_comp)
				{
					//SPC-компрессор
					unsigned char c = ch & 127;
					while (c--) {
						p2[0]=' ';
						p2++;
					}
					spc_comp = 0;
					continue;
				}
				if (tkn)
				{			
					p2[0] = 0;
					if (ch>=32 && ch<=0xc8) {
						strcat(p2,tokens[ch-32]);
						p2 += strlen(tokens[ch-32]);
						if (tkn & 4) {
							p2[0]=' ';
							p2++;
						}
					} else {
						strerr[0] = 0;
						wsprintf(strerr,"<Unknown token: #%02hX>",ch);
						strcat(p2,strerr);
						p2+=strlen(strerr);
					}
					tkn = 0;
					continue;
				}
				if (ch==0x0a) {
					continue;
				}
				if (ch==0x0d) {
					currentline++;
					p2[0]=0x0d;
					p2++;
					p2[0]=0x0a;
					p2++;
					if(buflen<=0) break;
					if (InsertNumbers) {
						p2[0] = 0;
						strerr[0] = 0;
						wsprintf(strerr,"%d:\t",currentline);
						strcat(p2,strerr);
						p2+=strlen(strerr);
					}
					continue;
				}
				if	(ch==0x06) {
					spc_comp = 1;
					continue;
				}
				if (ch>=2 && ch<=5) {
					tkn = ch;
					continue;
				}
				p2[0]=ch;
				p2++;
	}
	p2[0]=0;
	pout1 = (char*)malloc((p2-pout)+1);
	OemToChar(pout,pout1);
	free(pout);
	return pout1;
}

int lastloadtime=0;   // Workaround to RichEdit bug

int __stdcall ListNotificationReceived(HWND ListWin,int Message,WPARAM wParam,LPARAM lParam)
{
	switch (Message) {
	case WM_COMMAND:
		if (HIWORD(wParam)==EN_UPDATE) {
			int firstvisible=SendMessage(ListWin,EM_GETFIRSTVISIBLELINE,0,0);
			int linecount=SendMessage(ListWin,EM_GETLINECOUNT,0,0);
			if (linecount>0) {
				int percent=MulDiv(firstvisible,100,linecount);
				PostMessage(GetParent(ListWin),WM_COMMAND,MAKELONG(percent,itm_percent),(LPARAM)ListWin);
			}
			return 0;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_MEASUREITEM:
		break;
	case WM_DRAWITEM:
		break;
	}
	return 0;
}

HWND __stdcall ListLoadW(HWND ParentWin,WCHAR* FileToLoad,int ShowFlags)
{
	HWND hwnd;
	RECT r;
	DWORD w2;
	char *pdata;
	WCHAR *p;
	BOOL success=false;
	if (ShowFlags && lcp_forceshow==0) {  // don't check extension in this case!
		p=wcsrchr(FileToLoad,'\\');
		if (!p)
			return NULL;
		p=wcsrchr(p,'.');
		if (!p || (wcsicmp(p,supportedextension1)!=0 && wcsicmp(p,supportedextension2)!=0
			   && wcsicmp(p,supportedextension3)!=0 && wcsicmp(p,supportedextension4)!=0
			   && wcsicmp(p,supportedextension5)!=0 && wcsicmp(p,supportedextension6)!=0
			   && wcsicmp(p,supportedextension7)!=0 && wcsicmp(p,supportedextension8)!=0
			   && wcsicmp(p,supportedextension9)!=0))
			return NULL;
	}
	HANDLE f=CreateFileT(FileToLoad,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
							FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if (f==INVALID_HANDLE_VALUE)
		return NULL;

	if (!FLibHandle) {
		int OldError = SetErrorMode(SEM_NOOPENFILEERRORBOX);
		FLibHandle = LoadLibrary("Riched20.dll");
		if ((int)FLibHandle < HINSTANCE_ERROR) 
			FLibHandle = LoadLibrary("RICHED32.DLL");
		if ((int)FLibHandle < HINSTANCE_ERROR) 
			FLibHandle = NULL;
		SetErrorMode(OldError);
	}

	lastloadtime=GetCurrentTime();

	GetClientRect(ParentWin,&r);
	// Create window invisbile, only show when data fully loaded!
	hwnd=CreateWindow("RichEdit20A","",WS_CHILD | ES_MULTILINE | ES_READONLY |
		                        WS_HSCROLL | WS_VSCROLL | ES_NOHIDESEL,
		r.left,r.top,r.right-r.left,
		r.bottom-r.top,ParentWin,NULL,hinst,NULL);
	if (!hwnd)
		hwnd=CreateWindow("RichEdit","",WS_CHILD | ES_MULTILINE | ES_READONLY |
		                        WS_HSCROLL | WS_VSCROLL | ES_NOHIDESEL,
		r.left,r.top,r.right-r.left,
		r.bottom-r.top,ParentWin,NULL,hinst,NULL);
	if (hwnd) {
		// create an interesting font
		g_hf = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, RUSSIAN_CHARSET, 0, 0, 0, 0, TEXT("Courier New"));
		if (g_hf) {
			// set the font in the richedit control
            SendMessage(hwnd, WM_SETFONT, (WPARAM)g_hf, FALSE);
		}
		SendMessage(hwnd, EM_SETMARGINS, EC_LEFTMARGIN, 8);
		SendMessage(hwnd, EM_SETEVENTMASK, 0, ENM_UPDATE); //ENM_SCROLL doesn't work for thumb movements!
		PostMessage(ParentWin,WM_COMMAND,MAKELONG(lcp_ansi,itm_fontstyle),(LPARAM)hwnd);
		//Load file
		int l=GetFileSize(f,NULL);
		if (wcsstr(FileToLoad,L".$") || wcsstr(FileToLoad,L".!")) {
			//Hobbeta
			l-=17;
			SetFilePointer(f,17,0,FILE_BEGIN);
		}
		pdata=(char*)malloc(l+1);
		if (pdata) {
			ReadFile(f,pdata,l,&w2,NULL);
			if (w2<0) w2=0;
			if (w2>(DWORD)l) w2=l;
				if (CheckFormat(pdata,w2) && l <= 49152) {
					char *p2=Convert(pdata,w2,false);
					if (p2) {
						SetWindowText(hwnd,p2);
						free(p2);
						PostMessage(ParentWin,WM_COMMAND,MAKELONG(0,itm_percent),(LPARAM)hwnd);
						success=true;
					}
				}
			free(pdata);
		}
		if (!success) {
			DestroyWindow(hwnd);
			hwnd=NULL;
		}
	}
	CloseHandle(f);
	lastloadtime=GetCurrentTime();
	if (hwnd)
		ShowWindow(hwnd,SW_SHOW);
		SendMessage(hwnd, WM_SETFOCUS,0,0);
	return hwnd;
}

HWND __stdcall ListLoad(HWND ParentWin,char* FileToLoad,int ShowFlags)
{
	WCHAR FileToLoadW[wdirtypemax];
	return ListLoadW(ParentWin,awfilenamecopy(FileToLoadW,FileToLoad),ShowFlags);
}

int __stdcall ListLoadNextW(HWND ParentWin,HWND ListWin,WCHAR* FileToLoad,int ShowFlags)
{
	RECT r;
	DWORD w2;
	char *pdata;
	WCHAR *p;
	BOOL success=false;

	if (ShowFlags && lcp_forceshow==0) {  // don't check extension in this case!
		p=wcsrchr(FileToLoad,'\\');
		if (!p)
			return LISTPLUGIN_ERROR;
		p=wcsrchr(p,'.');
		if (!p || (wcsicmp(p,supportedextension1)!=0 && wcsicmp(p,supportedextension2)!=0
			   && wcsicmp(p,supportedextension3)!=0 && wcsicmp(p,supportedextension4)!=0
			   && wcsicmp(p,supportedextension5)!=0 && wcsicmp(p,supportedextension6)!=0
			   && wcsicmp(p,supportedextension7)!=0 && wcsicmp(p,supportedextension8)!=0
			   && wcsicmp(p,supportedextension9)!=0))
			return LISTPLUGIN_ERROR;
	}
	// Extension is supported -> load file
	HANDLE f=CreateFileT(FileToLoad,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
							FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if (f==INVALID_HANDLE_VALUE)
		return LISTPLUGIN_ERROR;

	lastloadtime=GetCurrentTime();

	GetClientRect(ParentWin,&r);
	// Create window invisbile, only show when data fully loaded!
	if (ListWin) {
		SetWindowText(ListWin,"");  // clear
		SendMessage(ListWin, EM_SETMARGINS, EC_LEFTMARGIN, 8);
		SendMessage(ListWin, EM_SETEVENTMASK, 0, ENM_UPDATE); //ENM_SCROLL doesn't work for thumb movements!

		PostMessage(ParentWin,WM_COMMAND,MAKELONG(lcp_ansi,itm_fontstyle),(LPARAM)ListWin);

		int l=GetFileSize(f,NULL);
		if (wcsstr(FileToLoad,L".$") || wcsstr(FileToLoad,L".!")) {
			//Hobbeta
			l-=17;
			SetFilePointer(f,17,0,FILE_BEGIN);
		}
		pdata=(char*)malloc(l+1);
		if (pdata) {
			ReadFile(f,pdata,l,&w2,NULL);
			if (w2<0) w2=0;
			if (w2>(DWORD)l) w2=l;
			pdata[w2]=0;
			if (strlen(pdata)==w2) {     // Make sure the file doesn't contain any 0x00 char!
				if (CheckFormat(pdata,w2)) {
					char *p2=Convert(pdata,w2,false);
					if (p2) {
						SetWindowText(ListWin,p2);
						free(p2);
						PostMessage(ParentWin,WM_COMMAND,MAKELONG(0,itm_percent),(LPARAM)ListWin);
						success=true;
					}
				}
			}
			free(pdata);
		}
		if (!success) {
			return LISTPLUGIN_ERROR;
		}
	}
	CloseHandle(f);
	lastloadtime=GetCurrentTime();
	return LISTPLUGIN_OK;
}

int __stdcall ListLoadNext(HWND ParentWin,HWND ListWin,char* FileToLoad,int ShowFlags)
{
	WCHAR FileToLoadW[wdirtypemax];
	return ListLoadNextW(ParentWin,ListWin,awfilenamecopy(FileToLoadW,FileToLoad),ShowFlags);
}

int __stdcall ListSendCommand(HWND ListWin,int Command,int Parameter)
{
	switch (Command) {
	case lc_copy:
		SendMessage(ListWin,WM_COPY,0,0);
		return LISTPLUGIN_OK;
	case lc_newparams:
		PostMessage(GetParent(ListWin),WM_COMMAND,MAKELONG(0,itm_next),(LPARAM)ListWin);
		return LISTPLUGIN_ERROR;
	case lc_selectall:
		SendMessage(ListWin,EM_SETSEL,0,-1);
		return LISTPLUGIN_OK;
	case lc_setpercent:
		int firstvisible=SendMessage(ListWin,EM_GETFIRSTVISIBLELINE,0,0);
		int linecount=SendMessage(ListWin,EM_GETLINECOUNT,0,0);
		if (linecount>0) {
			int pos=MulDiv(Parameter,linecount,100);
			SendMessage(ListWin,EM_LINESCROLL,0,pos-firstvisible);
			firstvisible=SendMessage(ListWin,EM_GETFIRSTVISIBLELINE,0,0);
			// Place caret on first visible line!
			int firstchar=SendMessage(ListWin,EM_LINEINDEX,firstvisible,0);
			SendMessage(ListWin,EM_SETSEL,firstchar,firstchar);
			pos=MulDiv(firstvisible,100,linecount);
			// Update percentage display
			PostMessage(GetParent(ListWin),WM_COMMAND,MAKELONG(pos,itm_percent),(LPARAM)ListWin);
			return LISTPLUGIN_OK;
		}
		break;
	}
	return LISTPLUGIN_ERROR;
}

int _stdcall ListSearchText(HWND ListWin,char* SearchString,int SearchParameter)
{
	FINDTEXT find;
	int StartPos,Flags;

	if (SearchParameter & lcs_findfirst) {
		//Find first: Start at top visible line
		StartPos=SendMessage(ListWin,EM_LINEINDEX,SendMessage(ListWin,EM_GETFIRSTVISIBLELINE,0,0),0);
		SendMessage(ListWin,EM_SETSEL,StartPos,StartPos);
	} else {
		//Find next: Start at current selection+1
		SendMessage(ListWin,EM_GETSEL,(WPARAM)&StartPos,0);
		StartPos+=1;
	}

	find.chrg.cpMin=StartPos;
	find.chrg.cpMax=SendMessage(ListWin,WM_GETTEXTLENGTH,0,0);
	Flags=0;
	if (SearchParameter & lcs_wholewords)
		Flags |= FR_WHOLEWORD;
	if (SearchParameter & lcs_matchcase)
		Flags |= FR_MATCHCASE;
	if (!(SearchParameter & lcs_backwards))
		Flags |= FR_DOWN;
	find.lpstrText=SearchString;
	int index=SendMessage(ListWin, EM_FINDTEXT, Flags, (LPARAM)&find);
	if (index!=-1) {
	  int indexend=index+strlen(SearchString);
	  SendMessage(ListWin,EM_SETSEL,index,indexend);
	  int line=SendMessage(ListWin,EM_LINEFROMCHAR,index,0)-3;
	  if (line<0)
		  line=0;
      line-=SendMessage(ListWin,EM_GETFIRSTVISIBLELINE,0,0);
      SendMessage(ListWin,EM_LINESCROLL,0,line);
	  return LISTPLUGIN_OK;
	} else {
		SendMessage(ListWin,EM_SETSEL,-1,-1);  // Restart search at the beginning
	}
	return LISTPLUGIN_ERROR;
}

void __stdcall ListCloseWindow(HWND ListWin)
{
	DestroyWindow(ListWin);
	return;
}

int __stdcall ListPrint(HWND ListWin,char* FileToPrint,char* DefPrinter,int PrintFlags,RECT* Margins)
{
	PRINTDLG PrintDlgRec;
	memset(&PrintDlgRec,0,sizeof(PRINTDLG));
	PrintDlgRec.lStructSize=sizeof(PRINTDLG);

    PrintDlgRec.Flags= PD_ALLPAGES | PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC;
	PrintDlgRec.nFromPage   = 0xFFFF; 
	PrintDlgRec.nToPage     = 0xFFFF; 
    PrintDlgRec.nMinPage	= 1;
    PrintDlgRec.nMaxPage	= 0xFFFF;
    PrintDlgRec.nCopies		= 1;
    PrintDlgRec.hwndOwner	= ListWin;// MUST be Zero, otherwise crash!
	if (PrintDlg(&PrintDlgRec)) 
	{
		HDC hdc=PrintDlgRec.hDC;
		DOCINFO DocInfo;
		POINT offset,physsize,start,avail,printable;
		int LogX,LogY;
		RECT rcsaved;

		// Warning: PD_ALLPAGES is zero!
		BOOL PrintSel=(PrintDlgRec.Flags & PD_SELECTION);
		BOOL PrintPages=(PrintDlgRec.Flags & PD_PAGENUMS);
		int PageFrom=1;
		int PageTo=0x7FFF;
		if (PrintPages) {
			PageFrom=PrintDlgRec.nFromPage;
			PageTo=PrintDlgRec.nToPage;
			if (PageTo<=0) PageTo=0x7FFF;
		}


		memset(&DocInfo,0,sizeof(DOCINFO));
		DocInfo.cbSize=sizeof(DOCINFO);
		DocInfo.lpszDocName=FileToPrint;
		if (StartDoc(hdc,&DocInfo)) {
			SetMapMode(hdc,MM_LOMETRIC);
			offset.x=GetDeviceCaps(hdc,PHYSICALOFFSETX);
			offset.y=GetDeviceCaps(hdc,PHYSICALOFFSETY);
			DPtoLP(hdc,&offset,1);
			physsize.x=GetDeviceCaps(hdc,PHYSICALWIDTH);
			physsize.y=GetDeviceCaps(hdc,PHYSICALHEIGHT);
			DPtoLP(hdc,&physsize,1);

			start.x=Margins->left-offset.x;
			start.y=-Margins->top-offset.y;
			if (start.x<0)
				start.x=0;
			if (start.y>0)
				start.y=0;
			avail.x=GetDeviceCaps(hdc,HORZRES);
			avail.y=GetDeviceCaps(hdc,VERTRES);
			DPtoLP(hdc,&avail,1);

			printable.x=min(physsize.x-(Margins->right+Margins->left),avail.x-start.x);
			printable.y=max(physsize.y+(Margins->top+Margins->bottom),avail.y-start.y);
  			
			LogX=GetDeviceCaps(hdc, LOGPIXELSX);
			LogY=GetDeviceCaps(hdc, LOGPIXELSY);

			SendMessage(ListWin, EM_FORMATRANGE, 0, 0);

			FORMATRANGE Range;
			memset(&Range,0,sizeof(FORMATRANGE));
			Range.hdc=hdc;
			Range.hdcTarget=hdc;
			LPtoDP(hdc,&start,1);
			LPtoDP(hdc,&printable,1);
			Range.rc.left = start.x * 1440 / LogX;
			Range.rc.top = start.y * 1440 / LogY;
			Range.rc.right = (start.x+printable.x) * 1440 / LogX;
			Range.rc.bottom = (start.y+printable.y) * 1440 / LogY;
			SetMapMode(hdc,MM_TEXT);

			BOOL PrintAborted=false;
			Range.rcPage = Range.rc;
			rcsaved = Range.rc;
			int CurrentPage = 1;
			int LastChar = 0;
			int LastChar2= 0;
			int MaxLen = SendMessage(ListWin,WM_GETTEXTLENGTH,0,0);
			Range.chrg.cpMax = -1;
			if (PrintPages) {
				do {
			        Range.chrg.cpMin = LastChar;
					if (CurrentPage<PageFrom) {
						LastChar = SendMessage(ListWin, EM_FORMATRANGE, 0, (LPARAM)&Range);
					} else {
						LastChar = SendMessage(ListWin, EM_FORMATRANGE, 1, (LPARAM)&Range);
					}
					// Warning: At end of document, LastChar may be<MaxLen!!!
					if (LastChar!=-1 && LastChar < MaxLen) {
						Range.rc=rcsaved;                // Check whether another page comes
						Range.rcPage = Range.rc;
						Range.chrg.cpMin = LastChar;
						LastChar2= SendMessage(ListWin, EM_FORMATRANGE, 0, (LPARAM)&Range);
						if (LastChar<LastChar2 && LastChar < MaxLen && LastChar != -1 &&
						  CurrentPage>=PageFrom && CurrentPage<PageTo) {
							EndPage(hdc);
						}
					}

					CurrentPage++;
					Range.rc=rcsaved;
					Range.rcPage = Range.rc;
				} while (LastChar < MaxLen && LastChar != -1 && LastChar<LastChar2 &&
					     (PrintPages && CurrentPage<=PageTo) && !PrintAborted);
			} else {
				if (PrintSel) {
					SendMessage(ListWin,EM_GETSEL,(WPARAM)&LastChar,(LPARAM)&MaxLen);
					Range.chrg.cpMax = MaxLen;
				}
				do {
					Range.chrg.cpMin = LastChar;
					LastChar = SendMessage(ListWin, EM_FORMATRANGE, 1, (LPARAM)&Range);

					// Warning: At end of document, LastChar may be<MaxLen!!!
					if (LastChar!=-1 && LastChar < MaxLen) {
						Range.rc=rcsaved;                // Check whether another page comes
						Range.rcPage = Range.rc;
						Range.chrg.cpMin = LastChar;
						LastChar2= SendMessage(ListWin, EM_FORMATRANGE, 0, (LPARAM)&Range);
						if (LastChar<LastChar2 && LastChar < MaxLen && LastChar != -1) {
							EndPage(hdc);
						}
					}
					CurrentPage++;
					Range.rc=rcsaved;
					Range.rcPage = Range.rc;
				} while (LastChar<LastChar2 && LastChar < MaxLen && LastChar != -1 && !PrintAborted);
			}
			if (PrintAborted)
				AbortDoc(hdc);
			else
				EndDoc(hdc);
		} //StartDoc
  
		SendMessage(ListWin, EM_FORMATRANGE, 0, 0);
		DeleteDC(PrintDlgRec.hDC);
	}
	if (PrintDlgRec.hDevNames)
		GlobalFree(PrintDlgRec.hDevNames);
	return 0;
}

void __stdcall ListGetDetectString(char* DetectString,int maxlen)
{
	strlcpy(DetectString,parsefunction,maxlen);
}

void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
}

int _stdcall ListSearchDialog(HWND ListWin,int FindNext)
{
	return LISTPLUGIN_ERROR;  // use ListSearchText instead!
}
