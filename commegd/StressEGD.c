#include <windows.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include "StressEGD.h"
#include "CommEGD.h"
#define USE_HIGH_RESOLUTION_TIMER	// need winmm.lib for the timeGetTime() function
//#include "gefcomm.h"
BOOL  CALLBACK gefStressEGDDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
int gefLoadSelectEGDExchangeList(HWND hListView, char *pNewDevice, int Period, int ByteLength);
LRESULT CALLBACK WndProc      (HWND, UINT, WPARAM, LPARAM) ;
void __cdecl gefEthernetThreadEGD(void);
static char szAppName[] = "StressEGD" ;
static const char *pSectionTypes[]={"CPU","PLC","GIO","EIO","CIO","DIO","PIO","AIO"};
static int ExchangeCount,AdjustPeriodPercent;
static GEF_EGD_DATA MessageEGD;
typedef struct {
	DWORD	TargetTCPIP;
	long	DeviceNumberType;	// 100*Number + Type
	long	ProducerPeriod;
	long	DataByteLength;
	DWORD	TickMessageSent;
//	Remaining items cleared by Reset button
	long	MessageCountSent;
	long	MessageCountDisplayed;
	long	MessageCountReply;
	DWORD	MinMessageTime;
	DWORD	MaxMessageTime;
	DWORD	TotalMessageTime;	// Total MS, divive by MessageCount for Avg
} GEF_EGD_EXCHANGE_STRESS;
#define GEF_MAX_EXCHANGE_COUNT 1000
GEF_EGD_EXCHANGE_STRESS gefExchangeStress[GEF_MAX_EXCHANGE_COUNT];
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,PSTR szCmdLine, int iCmdShow)
{
     MSG        msg ;
     HACCEL     hAccel ;
     WNDCLASSEX wndclass ;
	 HANDLE hMutex,hwndMain;
	WSADATA wsaData;
	int NoTCPIPAccess;

//	Limit to a single copy of the program
	 hMutex = CreateMutex(NULL, TRUE, "GEFanucStressEGD");
	 if (hMutex) {
		 if (GetLastError()==ERROR_ALREADY_EXISTS) {
			 MessageBox(NULL,"GE Fanuc StressEGD already running","Duplicate program",MB_OK|MB_ICONEXCLAMATION);
			return(0);
		 }
	 }
//	Check if TCP/IP network available
	 NoTCPIPAccess = TRUE;
	if (GetSystemMetrics(SM_NETWORK)) {
		NoTCPIPAccess = WSAStartup( MAKEWORD(1,1), &wsaData );
	} 
	if (NoTCPIPAccess) {
		MessageBox(NULL,"Not able to access TCP/IP network","Error, Need TCP/IP",MB_OK|MB_ICONSTOP);
		return(0);
	}
	 InitCommonControls();
	wndclass.cbSize        = sizeof (wndclass) ;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wndclass.lpfnWndProc   = WndProc ;
     wndclass.cbClsExtra    = 0 ;
     wndclass.cbWndExtra    = 0 ;
     wndclass.hInstance     = hInstance ;
     wndclass.hIcon         = LoadIcon (hInstance, "STRESSEGD");
     wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
     wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH) ;
     wndclass.lpszMenuName  = szAppName;
     wndclass.lpszClassName = szAppName ;
     wndclass.hIconSm       = LoadIcon (hInstance, szAppName) ;
	RegisterClassEx (&wndclass) ;

	hwndMain = CreateWindow (szAppName, NULL,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,
		CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT,	NULL, NULL, hInstance, szCmdLine) ;
//	Minimize main window immediately and start the Stress EGD dialog	
	ShowWindow (hwndMain, SW_SHOWMINIMIZED) ;
	SetWindowText(hwndMain,"StressEGD");
     hAccel = LoadAccelerators (hInstance, szAppName) ;
	SendMessage(hwndMain,WM_COMMAND, IDD_EGD_STRESS,0);
	while (GetMessage (&msg, NULL, 0, 0)) {
//		if (hDlgModeless == NULL || !IsDialogMessage (hDlgModeless, &msg)) {
//			if (!TranslateAccelerator (hwndMain, hAccel, &msg)) {
				TranslateMessage (&msg) ;
				DispatchMessage (&msg) ;
//			}
//		}
	}
	if (hMutex) {
		ReleaseMutex(hMutex);
	}
	if (GetSystemMetrics(SM_NETWORK)) {
	    WSACleanup( );
	}
	return msg.wParam ;
}
LRESULT CALLBACK WndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
     static HINSTANCE hInst ;

	if ((iMsg==WM_COMMAND)&&(LOWORD(wParam)==IDD_EGD_STRESS)) {
		DialogBox (hInst, MAKEINTRESOURCE(IDD_EGD_STRESS), hwnd, gefStressEGDDlgProc) ;
		return 0;
	}
    if (iMsg==WM_CLOSE) {
		DestroyWindow (hwnd) ;
		return 0 ;
	}
	if (iMsg==WM_DESTROY) {
		PostQuitMessage (0) ;
		return 0 ;
	}
	return DefWindowProc (hwnd, iMsg, wParam, lParam) ;
}
int gefResizeBottomListBox(HWND hDlg, int DlgWidthInit, int DlgBottomInit, int DlgLeftLast, int ListControlLeft, 
		int ListControlRight, LPARAM lParam)
/*
*	Routine to resize the list box, tree view or record view at the bottom of the dialog
*	It returnes the new height of the list/treeview box at the bottom in pixels
*	
*/
{
	HWND hItem;
	WORD Height,Width;
	RECT DlgRect,LeftRect,RightRect;
	GetWindowRect(hDlg,&DlgRect);
	Height = HIWORD(lParam);
	Width = LOWORD(lParam);
	if (Width>DlgWidthInit) {
		Width = DlgWidthInit;
		if ((DlgRect.left>=0)||(DlgRect.top>=0)) {
			SetWindowPos(hDlg, NULL, 0,0, Width,Height, SWP_NOMOVE|SWP_NOZORDER) ;
		}
		else {
			SetWindowPos(hDlg, NULL, DlgLeftLast,0, Width,Height, SWP_NOZORDER) ;
		}
	}
//			MoveWindow (hDlg, DlgRect.left, DlgRect.top, Width,HIWORD (lParam), TRUE) ;
	hItem = GetDlgItem(hDlg,ListControlLeft);
	GetWindowRect(hItem,&LeftRect);
	Height -= DlgBottomInit;
//	Height += GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);
	SetWindowPos(hItem, NULL, 0,0, LeftRect.right - LeftRect.left,Height, SWP_NOMOVE|SWP_NOZORDER) ;
	if (ListControlRight) {
		hItem = GetDlgItem(hDlg,ListControlRight);
		GetWindowRect(hItem,&RightRect);
		Width -= (int)(LeftRect.right - LeftRect.left);
//		Height += GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);
		SetWindowPos(hItem, NULL, 0,0, Width,Height, SWP_NOMOVE|SWP_NOZORDER) ;
	}
	return (Height);
}
#ifdef FUTURE_STUFF
int gefConnectPLCLink(HWND hDlg, int SelectCOMPort, int DataRateParity)
/*
*	Routine to start PLC communications
*	SelectCOMPort is COM port 1 to N, add 0x100 for dialout or = 0x1000 for 
*/
{
//	static GEF_NETWORK NetworkSetup; //,NetworkActive;
	static char SendBuffer[128],ReceiveBuffer[2048];
//				gefSetupCOMPortChange(hDlg, FALSE, &NetworkSetup);
				memset(&gefThreadData,0,sizeof(gefThreadData));
				gefThreadData.pNetwork = &NetworkSetup;
				gefThreadData.hCaller = hDlg;
//				gefThreadData.hCaller = GetParent(hDlg);
				gefThreadData.msgRequestComplete = WM_USER;
				gefThreadData.Command = 0;
				gefThreadData.RequestPortTCPIP = gLM.SelectCOMPort;
//				hItem = GetDlgItem(hDlg,IDC_PLC_DATARATE);
//				Index = SendMessage(hItem,CB_GETCURSEL,0,0);
//				DataRate = SendMessage(hItem,CB_GETITEMDATA,Index,0);
//				hItem = GetDlgItem(hDlg,IDC_PLC_PARITY);
//				DataRate += SendMessage(hItem,CB_GETCURSEL,0,0);
				gefThreadData.RequestDataRateParity = DataRateParity;
				gefThreadData.pDataToSend = SendBuffer;
				gefThreadData.pDataReceived = ReceiveBuffer;
				gefThreadData.MaxReceiveBytes = sizeof(ReceiveBuffer);
				gefThreadData.FirstReadTimeout = 2000;
	return(0);
}
#endif
BOOL CALLBACK gefStressEGDDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	int Loop,Index,Length,ControlNotify,Value,Delta,hFile,Enable;
	HWND hItem,hListView;
	WORD wID,wNotify;
	RECT DlgRect,ListRect;
	NMLISTVIEW *pNotifyHeader;
	GEF_EGD_EXCHANGE_STRESS *pExchangeStress;
	DWORD TickElapsed,TCPIP;
	HOSTENT *pHostEntry;
	struct in_addr TCPIPAddress;
	char Text[256],*pText;
	static int DlgWidthInit,DlgBottomInit,DlgLeftLast;
	static char *pAdjustTypes[] = {"Use Defined Period","Half Period/Double Rate","Double Period/Half Rate","Period=0/Maximum Rate"};
	static long AdjustPeriodPercentValues[] = {100,50,200,0};
	static DWORD TickStartup;
	static int SelectListViewIndex=0;

	switch (iMsg)  {
	case WM_INITDIALOG :
		GetWindowRect(hDlg,&DlgRect);
		DlgWidthInit = DlgRect.right - DlgRect.left;
		DlgLeftLast = DlgRect.left;
		hItem = GetDlgItem(hDlg, IDC_EGD_LIST);
		GetWindowRect(hItem,&ListRect);
		DlgBottomInit = ListRect.top - DlgRect.top - GetSystemMetrics(SM_CYCAPTION) - GetSystemMetrics(SM_CYFRAME);

		hListView = GetDlgItem(hDlg,IDC_EGD_LIST);
		ExchangeCount = gefLoadSelectEGDExchangeList(hListView,NULL,0,0);
//	Can adjust all exchange periods to be slower or faster than definitions, or as fast as possible
		hItem = GetDlgItem(hDlg,IDC_EGD_ADJUST_PERIOD);
		SendMessage(hItem,CB_RESETCONTENT,0,0);
		Length = sizeof(pAdjustTypes)/sizeof(pAdjustTypes[0]);
		for (Loop=0; Loop<Length; Loop++) {
			SendMessage(hItem,CB_ADDSTRING,0,(LPARAM)pAdjustTypes[Loop]);
		}
		SendMessage(hItem,CB_SETCURSEL,0,0);
		AdjustPeriodPercent = AdjustPeriodPercentValues[0];
		ListView_SetItemState(hListView,SelectListViewIndex,LVIS_SELECTED,LVIS_SELECTED);
//	Place computer name and TCP/IP address in title bar
		memcpy(Text,"Computer is ",13);
		gethostname(&Text[12],sizeof(Text)-12);
		pHostEntry = gethostbyname(&Text[12]);
		if (pHostEntry) {
			memcpy(&TCPIPAddress,pHostEntry->h_addr_list[0],sizeof(DWORD));
			pText = (char *)inet_ntoa(TCPIPAddress);
			Length = strlen(Text);
			memcpy(&Text[Length]," at ",4);
			strcpy(&Text[Length+4],pText);
		}
		SetWindowText(hDlg,Text);
		SetTimer(hDlg, 1, 100, NULL);
//	GetTickCount resolution is 10 MS under NT/2000 or 55 MS under Windows 9x
//	timeGetTime resolution defaults to 5 MS under NT/2000 or 1 MS under Windows 9x, needs winmm.lib
#ifdef USE_HIGH_RESOLUTION_TIMER
		TickStartup = timeGetTime();	
#else
		TickStartup = GetTickCount();
#endif
		if (ExchangeCount>0) {
			hItem = GetDlgItem(hDlg,IDC_EGD_RESET);
		}
		else {
			hItem = GetDlgItem(hDlg,IDC_EGD_TO_DEVICE);
		}
		SetFocus(hItem);
		_beginthread(gefEthernetThreadEGD,0,NULL);
        return FALSE ;

	case WM_SIZE:
		if ((wParam==SIZE_MAXIMIZED)||(wParam==SIZE_RESTORED)) {
			gefResizeBottomListBox(hDlg, DlgWidthInit, DlgBottomInit, DlgLeftLast,IDC_EGD_LIST, 0, lParam);
		}
		break;
	case WM_TIMER:
/*
		Value = AdjustPeriodPercent;
		gefEthernetThreadEGD();
		AdjustPeriodPercent = Value;
*/
//	Periodically update the exchange lines with changed message counts
		pExchangeStress = gefExchangeStress;
		hListView = GetDlgItem(hDlg,IDC_EGD_LIST);
		for (Index=0; Index<ExchangeCount; Index++) {
			Value = pExchangeStress->MessageCountSent + pExchangeStress->MessageCountReply;
			if (pExchangeStress->MessageCountDisplayed!=Value) {
				pExchangeStress->MessageCountDisplayed = Value;
#ifdef USE_HIGH_RESOLUTION_TIMER
				TickElapsed = timeGetTime() - TickStartup;
#else
				TickElapsed = GetTickCount() - TickStartup;
#endif
				Value = 0;
				if (TickElapsed) {
					Value = pExchangeStress->MessageCountSent*60000/TickElapsed;
				}
				Delta = pExchangeStress->MessageCountSent - pExchangeStress->MessageCountReply;
				wsprintf(Text,"%u / %u / %u",pExchangeStress->MessageCountSent,Value,Delta);
				ListView_SetItemText(hListView, Index, 3, Text);
				Value = 0;
				if (pExchangeStress->MessageCountReply) {
					Value = pExchangeStress->TotalMessageTime/pExchangeStress->MessageCountReply;
				}
				if (Value,pExchangeStress->MaxMessageTime>0) {
					wsprintf(Text,"%u / %u / %u",pExchangeStress->MinMessageTime,Value,pExchangeStress->MaxMessageTime);
				}
				else {
					strcpy(Text,"Exchange not active");
				}
				ListView_SetItemText(hListView, Index, 4, Text);
				ListView_Update(hListView, Index);
			}
			pExchangeStress++;
		}
		break;
/*
*	New common controls use NOTIFY. DEBUG  Columns will change in future when Off-Line
*/
	case WM_NOTIFY:
		pNotifyHeader = (NMLISTVIEW *)lParam;
		ControlNotify = pNotifyHeader->hdr.code;
		if ((ControlNotify==LVN_ITEMCHANGED)&&(pNotifyHeader->uNewState>0)) {
			SelectListViewIndex = pNotifyHeader->iItem;
			if (SelectListViewIndex<ExchangeCount) {
				Enable = FALSE;
				ListView_GetItemText(pNotifyHeader->hdr.hwndFrom, pNotifyHeader->iItem,0, Text,sizeof(Text));
			}
			else {
				Text[0] = '\0';
				Enable = TRUE;
			}
			SetDlgItemText(hDlg,IDC_EGD_TO_DEVICE,Text);
			EnableWindow(GetDlgItem(hDlg,IDC_EGD_TO_DEVICE),Enable);
			ListView_GetItemText(pNotifyHeader->hdr.hwndFrom, pNotifyHeader->iItem,1, Text,sizeof(Text));
			SetDlgItemText(hDlg,IDC_EGD_PERIOD,Text);
			ListView_GetItemText(pNotifyHeader->hdr.hwndFrom, pNotifyHeader->iItem,2, Text,sizeof(Text));
			SetDlgItemText(hDlg,IDC_EGD_BYTES,Text);
			EnableWindow(GetDlgItem(hDlg,IDC_EGD_SAVE),Enable);
		}
		break;

	case WM_COMMAND :
		wID = LOWORD(wParam);
		wNotify = HIWORD(wParam);
		if (wNotify==BN_CLICKED) {
			switch (wID) {
			case IDC_EGD_RESET:
//	Reset dynamic values. Note this is based on 6 DWORDS at end to the structure. Keep first 4
				pExchangeStress = gefExchangeStress;
				for (Index=0; Index<ExchangeCount; Index++) {
					memset(&pExchangeStress->MessageCountSent,0,6*sizeof(long));
					pExchangeStress->MessageCountDisplayed = -1;
					pExchangeStress++;
				}
#ifdef USE_HIGH_RESOLUTION_TIMER
				TickStartup = timeGetTime();	
#else
				TickStartup = GetTickCount();
#endif
				break;
			case IDC_EGD_SAVE:
				Value = GetDlgItemInt(hDlg,IDC_EGD_PERIOD,NULL,FALSE);
				Value = 10*((Value + 9)/10) ;
				if (Value<=0) {
					MessageBox(NULL,"A Producer Period from 10 to 3600000 Millisec is required\nA default of 1000 will be used","Need Producer Period",MB_OK);
					Value = 1000;
				}
				Length = GetDlgItemInt(hDlg,IDC_EGD_BYTES,NULL,FALSE);
				if (Length>1400) {
					Length = 1400;
				}
				if (Length<=0) {
					Length = 128;
				}
				hListView = GetDlgItem(hDlg,IDC_EGD_LIST);
				if (SelectListViewIndex<ExchangeCount) {
					pExchangeStress = gefExchangeStress;
					pExchangeStress += SelectListViewIndex;
					pExchangeStress->ProducerPeriod = Value;
					itoa(Value,Text,10);
					ListView_SetItemText(hListView, SelectListViewIndex, 1, Text);
					pExchangeStress->DataByteLength = Length;
					itoa(Length,Text,10);
					ListView_SetItemText(hListView, SelectListViewIndex, 2, Text);
				}
				else {
					if (GetDlgItemText(hDlg,IDC_EGD_TO_DEVICE,Text,sizeof(Text))<=1) {
						MessageBox(NULL,"A TCP/IP address or DNS name is required\nin the field after the To in the Exchange box","Need TCP/IP Address",MB_OK);
						SetFocus(GetDlgItem(hDlg,IDC_EGD_TO_DEVICE));
						break;
					}
					ExchangeCount = gefLoadSelectEGDExchangeList(hListView,Text,Value,Length);
				}
				EnableWindow(GetDlgItem(hDlg,IDC_EGD_SAVE),FALSE);
				break;
			case IDC_EGD_PING:
//	Can ping to selected exchange or to name or address entered in To field
				memcpy(Text,"Ping ",5);
				if (SelectListViewIndex<ExchangeCount) {
					pExchangeStress = gefExchangeStress;
					pExchangeStress += SelectListViewIndex;
					memcpy(&TCPIPAddress,&pExchangeStress->TargetTCPIP,sizeof(DWORD));
					pText = inet_ntoa(TCPIPAddress);
					strcpy(&Text[5],pText);
				}
				else {
					Length = GetDlgItemText(hDlg,IDC_EGD_TO_DEVICE,&Text[5],sizeof(Text)-5);
					if (Length<=1) {
						MessageBox(NULL,"Select Exchange line or enter address or name after To","Need TCP/IP Address or name",MB_OK);
						SetFocus(GetDlgItem(hDlg,IDC_EGD_TO_DEVICE));
						break;
					}
				}
				Length = strlen(Text);
				strcpy(&Text[Length],">\\TEMP\\PingData.txt");
				system(Text);
				hFile = _open("\\TEMP\\PingData.txt",_O_TEXT|_O_RDONLY,0);
				pText = (char *)_alloca(1024);
				Length = _read(hFile,pText,1023);
				_close(hFile);
				*(pText+Length) = '\0';
				MessageBox(NULL,pText,"Results of the Ping Command",MB_OK);
				break;
			case IDC_EGD_CLOSE:
			case IDCANCEL:
				AdjustPeriodPercent = -1;
				KillTimer(hDlg,1);
				EndDialog (hDlg, 0);
				PostQuitMessage (0);
				return TRUE ;
			default:
				break;
			}
		}
		if (wNotify==EN_CHANGE) {
			switch (wID) {
			case IDC_EGD_TO_DEVICE:
			case IDC_EGD_PERIOD:
			case IDC_EGD_BYTES:
				EnableWindow(GetDlgItem(hDlg,IDC_EGD_SAVE),TRUE);
			default:
				break;
			}
		}
		if (wNotify==CBN_SELCHANGE) {
			switch (wID) {
			case IDC_EGD_ADJUST_PERIOD:
				hItem = GetDlgItem(hDlg,IDC_EGD_ADJUST_PERIOD);
				Index = SendMessage(hItem,CB_GETCURSEL,0,0);
				AdjustPeriodPercent = AdjustPeriodPercentValues[Index];
				break;
			case IDC_EGD_DEVICES:
				hItem = GetDlgItem(hDlg,IDC_EGD_DEVICES);
				Index = SendMessage(hItem,CB_GETCURSEL,0,0);
				TCPIP = SendMessage(hItem,CB_GETITEMDATA,Index,0);
				memcpy(&TCPIPAddress,&TCPIP,sizeof(DWORD));
				pText = inet_ntoa(TCPIPAddress);
				SetDlgItemText(hDlg,IDC_EGD_TO_DEVICE,pText);
			default:
				break;
			}
		}
		break ;
	}
	return FALSE ;
}
#ifdef SCREEN_INFO
IDD_EGD_STRESS DIALOG DISCARDABLE  20, 20, 274, 194
    COMBOBOX        IDC_EGD_DEVICES,1,0,70,114,CBS_DROPDOWNLIST | WS_VSCROLL | 
    COMBOBOX        IDC_EGD_ADJUST_PERIOD,125,0,68,87,CBS_DROPDOWNLIST | 
    DEFPUSHBUTTON   "&Reset",IDC_EGD_RESET,205,1,33,13
    PUSHBUTTON      "&Close",IDC_EGD_CLOSE,241,1,33,13
    PUSHBUTTON      "&Ping",IDC_EGD_PING,241,20,27,12,WS_DISABLED
    PUSHBUTTON      "&Save",IDC_EGD_SAVE,210,20,27,12
    EDITTEXT        IDC_EGD_TO_DEVICE,15,20,63,12,ES_AUTOHSCROLL
    EDITTEXT        IDC_EGD_PERIOD,118,20,36,12,ES_AUTOHSCROLL | ES_NUMBER
    EDITTEXT        IDC_EGD_BYTES,180,20,24,12,ES_AUTOHSCROLL | ES_NUMBER
    CONTROL         "List1",IDC_EGD_LIST,"SysListView32",LVS_REPORT | 
#endif
int gefLoadSelectEGDExchangeList(HWND hListView, char *pNewDevice, int Period, int ByteLength)
{
	int Column,MaxPixelWidth,Status,SelectIndex;
	int ExchangeCount,StressCount,DeviceCount,Index;
	DWORD LastTCPIP;
	HOSTENT *pHostEntry;
	RECT Rect;
	HWND hItem;
	LVCOLUMN lvColumn;
	LVITEM lvItem;
	char Text[256];
	GEF_EGD_EXCHANGE_CONFIG ExchangeConfig;
	GEF_EGD_EXCHANGE_STRESS *pExchangeStress;
//	GEF_FOLDER_LIST *pFolderList;
//	GEF_PLC_INI_LIST *pDeviceList;
	static int CurrentExchangeCount=0;
	int MaxColumnInserted;
	static char *pExchangeHeader[]={"Device","Period","Bytes","Sent/PerMin/Lost","EchoMS Min/Avg/Max"};
	static short ExchangeWidthPercent[]={24,10,10,25,30};

	GetWindowRect(hListView, &Rect);
	MaxPixelWidth = Rect.right - Rect.left;
	memset(&lvColumn,0,sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;
	MaxColumnInserted = sizeof(pExchangeHeader)/sizeof(pExchangeHeader[0]);

	memset(&lvItem,0,sizeof(LVITEM));
	lvItem.mask = LVIF_TEXT;
	if (pNewDevice) {
//	If pointer to TCP/IP address or computer name string, add to list
		LastTCPIP = inet_addr(pNewDevice);
		if (LastTCPIP==INADDR_NONE) {
			pHostEntry = gethostbyname(pNewDevice);
			if (pHostEntry) {
				memcpy(&LastTCPIP,pHostEntry->h_addr_list[0],sizeof(DWORD));
			}
			else {
				wsprintf(Text,"Device at %s\nis not a recognized on this network",pNewDevice);
				MessageBox(NULL,Text,"Error, Unknown TCP/IP Address",MB_OK|MB_ICONSTOP);
				return(0);
			}
		}
		if (CurrentExchangeCount<GEF_MAX_EXCHANGE_COUNT) {
			pExchangeStress = gefExchangeStress;
			pExchangeStress += CurrentExchangeCount;
			memset(pExchangeStress,0,sizeof(GEF_EGD_EXCHANGE_STRESS));
			pExchangeStress->TargetTCPIP = LastTCPIP;
			pExchangeStress->ProducerPeriod = Period;
			pExchangeStress->DataByteLength = ByteLength;
			pExchangeStress->MessageCountDisplayed = -1;
			lvItem.iItem = CurrentExchangeCount;
			lvItem.iSubItem = 0;
			lvItem.pszText = pNewDevice;
			ListView_InsertItem(hListView, &lvItem);
			itoa(Period,Text,10);
			ListView_SetItemText(hListView, CurrentExchangeCount, 1, Text);
			itoa(ByteLength,Text,10);
			ListView_SetItemText(hListView, CurrentExchangeCount, 2, Text);
			CurrentExchangeCount++;
		}
	}
	else {
//	Otherwise clear recoved view and load producers from gefComm.ini file
		ListView_DeleteAllItems(hListView);
		SelectIndex = 0;
		LastTCPIP = 0;
		ExchangeCount = gefEGDLoadConfig(0, GEF_MAX_EXCHANGE_COUNT*2);
		hItem = GetDlgItem(GetParent(hListView),IDC_EGD_DEVICES);
		SendMessage(hItem,CB_RESETCONTENT,0,0);
		DeviceCount = 0;
		for (Column=0; Column<MaxColumnInserted; Column++) {
			lvColumn.pszText = pExchangeHeader[Column];
			lvColumn.iSubItem = Column;
			lvColumn.cx = ExchangeWidthPercent[Column]*MaxPixelWidth/100;
			Status = ListView_InsertColumn(hListView,Column,&lvColumn);
		}
		ExchangeCount = gefEGDLoadConfig(0, GEF_MAX_EXCHANGE_COUNT*2);
		pExchangeStress = gefExchangeStress;
		StressCount = 0;
		for (Index=0; Index<ExchangeCount; Index++) {
			gefEGDConfig(FALSE, Index, &ExchangeConfig, 0, NULL);
/*
		pExchangeConfig->ProducerTCPIP = pExchangeList->ProducerTCPIP;	
		pExchangeConfig->ExchangeID = pExchangeList->ExchangeID;
		if (pExchangeList->ConsumerTCPIP==gefEGDThreadData.LoadTCPIP) {
			pExchangeConfig->ConsumerTCPIP = 0;
			DeviceType = (short)(pExchangeList->ProducerDeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE);
			DeviceNumber = (short)(pExchangeList->ProducerDeviceTypeNumber%GEF_SHIFT_DEVICE_TYPE);
		}
		else {
			pExchangeConfig->ConsumerTCPIP = pExchangeList->ConsumerTCPIP;	
			DeviceType = (short)(pExchangeList->ConsumerDeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE);
			DeviceNumber = (short)(pExchangeList->ConsumerDeviceTypeNumber%GEF_SHIFT_DEVICE_TYPE);
		}
*/
			if (!ExchangeConfig.ConsumerTCPIP&&(StressCount<GEF_MAX_EXCHANGE_COUNT)) {
				memset(pExchangeStress,0,sizeof(GEF_EGD_EXCHANGE_STRESS));
				pExchangeStress->TargetTCPIP = ExchangeConfig.ProducerTCPIP;
				pExchangeStress->DeviceNumberType = 100*((long)ExchangeConfig.DeviceNumber) + ExchangeConfig.DeviceType;
				pExchangeStress->ProducerPeriod = ExchangeConfig.ProducerPeriod;
//static const char *pSectionTypes[]={"CPU","PLC","GIO","EIO","CIO","DIO","PIO","AIO"};
//				pExchangeConfig->ConsumerTimeout = pExchangeList->ConsumerTimeout;
				pExchangeStress->DataByteLength = ExchangeConfig.DataByteLength;
				pExchangeStress->MessageCountDisplayed = -1;
//				pExchangeConfig->MemoryListCount = pExchangeList->AddressSegmentCount;
//				pExchangeConfig->PLCStatusTypeAddress = pExchangeList->PLCStatusTypeAddress;	
//				pExchangeConfig->DataBytesReceived = pExchangeList->DataBytesReceived;
				lvItem.iItem = StressCount;
				lvItem.iSubItem = 0;
				lvItem.pszText = Text;
				memcpy(Text,pSectionTypes[ExchangeConfig.DeviceType],3);
				_itoa(ExchangeConfig.DeviceNumber,&Text[3],10);
				ListView_InsertItem(hListView, &lvItem);
				if (pExchangeStress->TargetTCPIP!=LastTCPIP) {
					LastTCPIP = pExchangeStress->TargetTCPIP;
					SendMessage(hItem,CB_ADDSTRING,0,(LPARAM)Text);
					SendMessage(hItem,CB_SETITEMDATA,DeviceCount++,(LPARAM)LastTCPIP);
				}
				itoa(pExchangeStress->ProducerPeriod,Text,10);
				ListView_SetItemText(hListView, StressCount, 1, Text);
				itoa(pExchangeStress->DataByteLength,Text,10);
				ListView_SetItemText(hListView, StressCount, 2, Text);
				pExchangeStress++;
				StressCount++;
			}
		}
		if (DeviceCount<=0) {
			SendMessage(hItem,CB_ADDSTRING,0,(LPARAM)"No GEFComm.ini file");
			EnableWindow(hItem,FALSE);
		}
		SendMessage(hItem,CB_SETCURSEL,0,0);
		lvItem.iItem = StressCount;
		lvItem.iSubItem = 0;
		lvItem.pszText = "Next Exchange";
		ListView_InsertItem(hListView, &lvItem);
		CurrentExchangeCount = StressCount;
	}
//	This requires IE v3.0 
//	ListView_SetExtendedListViewStyle(hListView, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
	ListView_SetExtendedListViewStyle(hListView, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	return(CurrentExchangeCount);
}
void __cdecl gefEthernetThreadEGD(void)
{
	char Text[_MAX_PATH];
	HOSTENT *pHostEntry;
	GEF_EGD_EXCHANGE_STRESS *pExchangeStress,*pExchangeReply;
	struct in_addr HostAddress;
//	struct in_addr FromAddress;
	struct sockaddr_in FromSocketAddress;
	int Loop,CurrentExchange;
    long ByteLength,DataLength,FromLength;
	DWORD Period,TickCurrent,TickDelta;
	static SOCKET HostSocket=0,TargetSocket;
	static struct sockaddr_in HostSocketAddress,TargetSocketAddress;
	static DWORD HostAddressDWORD;

//	Get short and full host computer name and TCP/IP address
	if (!HostSocket) {
		memset(&MessageEGD,0,sizeof(MessageEGD));
		gethostname(Text,sizeof(Text));
		pHostEntry = gethostbyname(Text);
		memcpy(&HostAddress,pHostEntry->h_addr_list[0],sizeof(DWORD));
		memcpy(&HostAddressDWORD,&HostAddress,sizeof(DWORD));
//		pText = (char *)inet_ntoa(HostAddress);
//		printf("\nComputer is %s/(or %s) = %s",Text,pHostEntry->h_name,pText);
//	Create datagram socket and bind to port "GF" to receive EGD messages
	    HostSocket = socket(AF_INET, SOCK_DGRAM,0);
		TargetSocket = socket(AF_INET, SOCK_DGRAM,0);
		if ((HostSocket == INVALID_SOCKET)||(TargetSocket == INVALID_SOCKET)) {
			wsprintf(Text,"Error %d, Not able to create sockets. Check if TCP/IP networking installed",WSAGetLastError());
			MessageBox(NULL,Text,"Error starting TCP/IP thread",MB_OK|MB_ICONSTOP);
			return;
		}
		else {
			memset(&HostSocketAddress,0,sizeof(HostSocketAddress));
			HostSocketAddress.sin_family = AF_INET;
			HostSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
			if (bind(HostSocket, (LPSOCKADDR)&HostSocketAddress, sizeof(HostSocketAddress)) == SOCKET_ERROR) {
				wsprintf(Text,"Error %d, Not able to bind this computer socket to EGD data port GF",WSAGetLastError());
				MessageBox(NULL,Text,"Error starting TCP/IP thread",MB_OK|MB_ICONSTOP);
				return;
			}
			memcpy(&TargetSocketAddress,&HostSocketAddress,sizeof(HostSocketAddress));
		}
	}
	CurrentExchange = ExchangeCount;
	pExchangeStress = gefExchangeStress;
	while (AdjustPeriodPercent>=0) {
//	Check if any UDP datagrams on this port. If none, check for key, then give up this tick
		do {
			if (AdjustPeriodPercent<0) {
				break;
			}
			ioctlsocket (HostSocket, FIONREAD, &ByteLength);
			if (ByteLength>0) {
//	Received UDP datagram on this port
				FromLength = sizeof(FromSocketAddress);
			    ByteLength = recvfrom(HostSocket,(char *)&MessageEGD, sizeof(MessageEGD), 0, (LPSOCKADDR)&FromSocketAddress, &FromLength);
#ifdef USE_HIGH_RESOLUTION_TIMER
				TickCurrent = timeGetTime();	
#else
				TickCurrent = GetTickCount();
#endif
				if (ByteLength == SOCKET_ERROR) {
					wsprintf(Text,"Error %d receiving UDP datagram",WSAGetLastError());
					MessageBox(NULL,Text,"Error starting TCP/IP thread",MB_OK|MB_ICONINFORMATION);
			    } 
				else {
					DataLength = ByteLength - 32;
//	Check data length and Type and version for EGD data
					if ((DataLength>0)&&(MessageEGD.PDUTypeVersion==0x010D)) {
//						memcpy(&FromAddress,&MessageEGD.ProducerID,sizeof(DWORD));
//						wsprintf(Text,"\nReceived %u data bytes from %s Exchange %u, Request %u",
//							DataLength,(char *)inet_ntoa(FromAddress),MessageEGD.ExchangeID,MessageEGD.RequestID);
//						MessageBox(NULL,Text,"Received data",MB_OK);
						if (MessageEGD.TimeStampSec<(DWORD)ExchangeCount) {
							pExchangeReply = gefExchangeStress;
							pExchangeReply += MessageEGD.TimeStampSec;
							TickDelta = TickCurrent - pExchangeReply->TickMessageSent;
							if (!TickDelta) {
								TickDelta = 1;
							}
							if (TickDelta>pExchangeReply->MaxMessageTime) {
								pExchangeReply->MaxMessageTime = TickDelta;
							}
							if (pExchangeReply->MessageCountReply++) {
								if (TickDelta<pExchangeReply->MinMessageTime) {
									pExchangeReply->MinMessageTime = TickDelta;
								}
							}
							else {
								pExchangeReply->MinMessageTime = TickDelta;
							}
							pExchangeReply->TotalMessageTime += TickDelta;
							if (pExchangeReply->MessageCountReply>pExchangeReply->MessageCountSent) {
								pExchangeReply->MessageCountReply = pExchangeReply->MessageCountSent;
							}
						}
					}
				}
			}
		} while (ByteLength>0);
		for (Loop=0; Loop<ExchangeCount; Loop++) {
			if (AdjustPeriodPercent<0) {
				break;
			}
			pExchangeStress++;
			CurrentExchange++;
			if (CurrentExchange>=ExchangeCount) {
				pExchangeStress = gefExchangeStress;
				CurrentExchange = 0;
			}
			Period = (pExchangeStress->ProducerPeriod*AdjustPeriodPercent)/100;
#ifdef USE_HIGH_RESOLUTION_TIMER
			TickCurrent = timeGetTime();	
#else
			TickCurrent = GetTickCount();
#endif
			TickDelta = TickCurrent - pExchangeStress->TickMessageSent;
			if (TickDelta>=Period) {
				MessageEGD.PDUTypeVersion = 0x010D;
				MessageEGD.RequestID = (WORD)pExchangeStress->MessageCountSent;
				MessageEGD.ProducerID = HostAddressDWORD;
				MessageEGD.ExchangeID = CurrentExchange;
				MessageEGD.TimeStampSec = CurrentExchange;
				MessageEGD.TimeStampNanoSec = TickCurrent;
				MessageEGD.Status = 1;
				ByteLength = pExchangeStress->DataByteLength + 32;
				memcpy(&TargetSocketAddress.sin_addr,&pExchangeStress->TargetTCPIP,sizeof(DWORD));
				if (sendto(TargetSocket,(char *)&MessageEGD,ByteLength,0,(LPSOCKADDR)&TargetSocketAddress,sizeof(TargetSocketAddress))!=SOCKET_ERROR) {
					pExchangeStress->MessageCountSent++;
					pExchangeStress->TickMessageSent = TickCurrent;
//					memcpy(&FromAddress,&pExchangeStress->TargetTCPIP,sizeof(DWORD));
//					wsprintf(Text,"Send %u data bytes from %s Exchange %u, Request %u",
//							ByteLength,(char *)inet_ntoa(FromAddress),MessageEGD.ExchangeID,MessageEGD.RequestID);
//					MessageBox(NULL,Text,"Send Data",MB_OK);
				}
				else {
					wsprintf(Text,"Error %d sending UDP datagram",WSAGetLastError());
					MessageBox(NULL,Text,"Error sending EGD data",MB_OK|MB_ICONINFORMATION);
				}
				break;
			}
		}
//	Sleep for 1 MS may give up rest of tick, a 0 waits less, but locks out lower priority tasks
		Sleep(1);
	}
    if (HostSocket) closesocket(HostSocket);
	HostSocket = 0;
    if (TargetSocket) closesocket(TargetSocket);
	_endthread();
	return;
}
