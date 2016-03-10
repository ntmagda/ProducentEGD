/*
*   The gefAccessGENIHardware procedure is the interface routine to access the
*   hardware level Genius(and PCIF) driver for 32 bit Windows NT or Windows 95.
*   It also supports DOS/16 bit Windows access using the GENI_VDD or WSI_VDD
*   VDD's for 16 bit applications like LM6. Hardware information for up to 16 
*   PCIM/GENI cards and S6 or S90 WSI cards is defined in the Windows registry under
*	You can use the TestGENI program to define registry data or do it in your program.
*	
*   The GENICard.SYS driver opens the 16 KB shared RAM GENI memory as a
*   file. It also supports a number of IOCtl commands for I/O port and memory
*   access using the gefAccessGENIHardware routine. Parameters are
*
*   Status = gefAccessGENIHardware(int GENINumber, int Command,
*                           void *pParameter1, void *pParameter2);
*
*   GENINumber is an input from 1 to 16 to select a PCIM/GENI card for this Command
*   Win32 parameters must be defined in the Registry under \GENICard and
*   using the GENINumber after the PortAddress, MemorySegment and Interrupt names.
*
*   Command is an input number from the following list
*   pParameter1 and 2 are parameter pointers based on the Command
*   Command/Description     pParameter1 pointer					pParameter2 pointer
*   1   Open GENI Card      GENI registry data or NULL			NULL
*   2   Close GENI Card     NULL								NULL
*	3	Turn GENI On		NULL								NULL
*	4	Turn GENI Off		NULL								NULL
*   5   Read GENI LED       return short 5 bit Port A data		NULL
*   6   Change GENI Config  short GlobalLength,RefAddress		NULL
*   7   Read GENI Memory    short ByteOffset, ByteLength		pData pointer
*   8   Write GENI Memory   short ByteOffset, ByteLength		pData pointer
*   9   Transfer GENI Memory FUTURE structure with Ref tables & device Addr.
*	10	Send GENI Datagram	Datagram header						pData pointer
*	11	Read GENI Datagram	Datagram header						pData pointer
*	12	Define GENI Callback event number						pData pointer
*   13  Read GENI I/O Port  Short address out					data byte read from I/O port
*   14  Write GENI I/O Port Short address and data byte to write to I/O port
*   immediately. The calling program must wait the 1.8 to 2 seconds, then read
*   the LED status to see when the lights are on, then call ChangeGENIConfig
*   to update the broadcast length and other info from the Windows registry.
*   Existing applications such as GENILIB, can access memory using a GENI_MEMORY
*   structure pointing to the pGENIMemory address returned by Open(1), New programs
*   should use the Read and Write GENI memory (7/8/9) commands to access GENI memory
*   as they wait for the GENI memory interlock for over one byte I/O transfers.
*   Comand 7 querys the registry while 8 is for a FUTURE 
*   Commands 9 and 10 are for FUTURE interrupts while 11 and 12 are for the 32
*   bit TESTGENI program to update EEPROM config data on new PCIM cards.
*   The function returns a 0 or a positive number like a byte count for Success
*	or a negative Error number
*/
#ifdef WIN32
#include <windows.h>
#include <winioctl.h>
#endif
#include <stdio.h>		/* for printf */
#include <stdlib.h>	
#define MAX_GENI_DEVICE_NUMBER 16
#define PCIM_SHARED_MEMORY_SIZE 16384
#define MAX_GENICARD_QUEUE_MESSAGES 16
#define MAX_GENICARD_QUEUE_BYTES 2464
#include "gefcomm.h"
#include "genilib.h"
/*
#include "gefctrl.h"
*/
#ifndef WIN32
#define memcpy _fmemcpy
#define memcmp _fmemcmp
#define memset _fmemset
#define memmove _fmemmove
#endif
/*#include <crtdbg.h>		/* for _ASSERTE */
#ifdef LOCAL_DEBUG_MODE
BOOL DeviceIoControlX(
    HANDLE hDevice,	/* handle to device of interest */
    DWORD dwIoControlCode,	/* control code of operation to perform */
    LPVOID lpInBuffer,	/* pointer to buffer to supply input data */
    DWORD nInBufferSize,	/* size of input buffer */
    LPVOID lpOutBuffer,	/* pointer to buffer to receive output data */
    DWORD nOutBufferSize,	/* size of output buffer */
    LPDWORD lpBytesReturned,	/* pointer to variable to receive output byte count */
    LPVOID lpOverlapped 	/* pointer to overlapped structure for asynchronous operation */
   );
*/
#endif         
/*
//#include "GENICard.h"
// Define the IOCTL function codes using the CTL_CODE DDK macro
// #define CTL_CODE( DeviceType, Function, Method, Access ) (           \
//    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
// Define the Device Type values. Note Microsoft uses values in the range
// 0 to 0x7FFF, and 0x8000 to 0xFFFF are reserved for use by customers.
*/
#define DEVICE_TYPE_GENIUS  0x9000  /* was 0x9000 */
/*
*	GENICard IOCTL command codes. For WinNT, shift left 2 bits and add to base
*	The number order depends on the order of if statements in gef_gioc.c
*/
#define GENICARD_READ_IO_PORT	1
#define GENICARD_WRITE_IO_PORT	2
#define GENICARD_READ_PCIF_DATA 3
#define GENICARD_WRITE_PCIF_DATA 4
#define GENICARD_READ_PCIF_DMA  5
#define GENICARD_WRITE_PCIF_DMA 6
#define GENICARD_READ_MEMORY    7
#define GENICARD_WRITE_MEMORY   8
#define GENICARD_TRANSFER_MEMORY 9
#define GENICARD_SEND_DATAGRAM	10
#define GENICARD_READ_DATAGRAM	11
#define GENICARD_CHANGE_CONFIG	12
#define GENICARD_SET_WATCHDOG   13
#define GENICARD_FLAG_INTERRUPT 14
/*
*	Routine used for internal testing of gef_gioc.c as 32 bit app under Win95
*/
#ifdef TESTING_GEF_GIOC
DWORD gefTestIOCTLProcessing(DWORD IOCTLCommand, void *pInBuffer, DWORD cbInBuffer,
					void *pOutBuffer, DWORD cbOutBuffer, DWORD *pBytesReturned);
#define DeviceIoControl(H,C,I,LI,O,LO,R,L) gefTestIOCTLProcessing((C),(I),(LI),(O),(LO),(R))
#endif
#ifndef WIN32
int gefTranslateIOCTL(int IOCTLCommand, void far *pInBuffer, int cbInBuffer,
					void far *pOutBuffer, int cbOutBuffer, int *pBytesReturned);
#define DeviceIoControl(H,C,I,LI,O,LO,R,L) gefTranslateIOCTL((C),(I),(LI),(O),(LO),(R))
#endif
/*
// Macro definition for defining IOCTL and FSCTL function control codes.
// The IOCTL code contains a command identifier, plus other information about
// the device, the type of access with which the file must have been opened, 
// and the type of buffering. Note that function codes 0 to 0x7FF are reserved 
// for Microsoft Corp. and codes 0x800 to 0xFFF are reserved for customers.
// The Genius NT GENICard driver starts Function commands at 0x900 and adds
// GENICARD IOCTL command shifted left 2 bits to not overlay the Method.
// The Win 95 GENICard.VxD shifts the Network number (1 to 16) left 8 bits and
// adds it to the GENICard IOCTL command. Win 95 VxD's are totally different
// from Win NT drivers and they do not have different handles for each device
*/
#ifdef WIN32
#define IOCTL_GENIUS_BASE_FUNCTION  \
		CTL_CODE(DEVICE_TYPE_GENIUS, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#else
#define IOCTL_GENIUS_BASE_FUNCTION  0
#endif
enum {ErrorBadGENINumber=(-1000),ErrorGENINotOpen,ErrorBadGENICommand,
	ErrorNoGENIParameterPointer};
#pragma pack(1)
typedef struct {
	DWORD   MemoryBase; /* linear address for start of PCIM shared memory */
	WORD    IOPortBase;	/* PortAddress, low 2 bits to 1=ELB921, 2=ELB922 */
	WORD	PriorityGlobalByteLength;	/* Length 0-128 in low byte */
	short	GlobalReference;	
	HANDLE  GENIHandle;		/* Windows NT file handle, always same for Win 95 */
	OVERLAPPED BlockedEvent;	/* for future asynchronous I/O */
	GENICARD_QUEUE	Queue;	/* message counts and limits */
#ifdef STUFF_IN_GENICARD_ONLY
	BYTE	*pBufferStart;	/* buffer, which may not follow */
	BYTE	*pBufferNext;	/* buffer location for next message */
	GENICARD_MESSAGE *pBuffer[MAX_GENICARD_QUEUE_MESSAGES];
	BYTE	Buffer[MAX_GENICARD_QUEUE_BYTES];	/* 16 * (150 + 4) */
#endif
} GENICARD_SETUP_DATA;
typedef struct {
	DWORD   MemoryBase; /* linear address for start of PCIF2 shared memory */
	WORD    IOPortBase;
	WORD	PriorityGlobalByteLength;	/* WatchDog/Priority in hi byte */
	DWORD	RackTypes;		/* Rack 1 to 7 per nibble with 0 to 4 */
	HANDLE  PCIFHandle;		/* Windows NT file handle, always same for Win 95 */
/*	OVERLAPPED BlockedEvent;	// for future asynchronous I/O */
} PCIFCARD_SETUP_DATA;
typedef struct {
	HWND    hWindow;        /* Windows handle of program to handle interrupt */
	unsigned iMessage;      /* Genius interrupt message number sent to handle */
} GENIUS_INTERRUPT_MESSAGE;
#pragma pack()
/*
*	Local data private to hardware access routines. Note GENICARD_SETUP_DATA
*	and PCIFCARD_SETUP_DATA are NOT the same as used in the driver
*/
static BYTE gefPCIFRemoteModuleID[MAX_PCIF_CARD_NUMBER][8][11];  /* CardRackSlot */
static GENICARD_SETUP_DATA gefGENICardSetupData[MAX_GENI_DEVICE_NUMBER+1];
static PCIFCARD_SETUP_DATA gefPCIFCardSetupData[MAX_PCIF_CARD_NUMBER+1];
#ifdef WIN32
static HANDLE hVxD=NULL;
static int CountOpenVxD=0;
#endif
static char *pParameterName[6] = {"PortAddress","MemorySegment","GlobalByteLength",
			"GlobalReference","PriorityWatchdog","RackTypes"};
DllExport int WINAPI gefAccessGENIHardware(int GENINumber, int Command,
				 void far *pParameter1,	void far *pParameter2)
{
	HANDLE  hDeviceHandle;
 	int   IOCTLCommand,Network,ByteLength,cbReturned;
	WORD	IOPortWriteByte,MemoryOffset,IOPortReadByte,GENIStatusByte;
	BOOL    IOCTLStatus;
	int ReturnStatus;
	static int CompletedFirstCall=0;
	static int RunningWindowsNT; 
#ifdef WIN32
/*    static HANDLE hVxD=NULL; */
	char sFileName[20],sDigits[18];
	OSVERSIONINFO OSVersionInfo;
	int Loop;
#endif
	GENICARD_SETUP_DATA far *pGENICardSetupData;
	GENICARD_MESSAGE far *pGENICardMessage;
//char SaveLine[1000];

	if (!CompletedFirstCall) {
		CompletedFirstCall = 1;
		memset(gefGENICardSetupData,0,sizeof(gefGENICardSetupData));
/*
*	Determine the operating system. The Win 32 v4.0 choices are	VER_PLATFORM_WIN32s,
*	VER_PLATFORM_WIN32_WINDOWS, or VER_PLATFORM_WIN32_NT
*/
		RunningWindowsNT = FALSE;
#ifdef WIN32
		OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&OSVersionInfo);
		if (OSVersionInfo.dwPlatformId==VER_PLATFORM_WIN32_NT) {
			RunningWindowsNT = TRUE;
		}
#endif
	}
/*  Check that Command and GENINumber, (1 to 16 for GENI) are in range */
	if ((GENINumber<0)||(GENINumber>MAX_GENI_DEVICE_NUMBER)) {
/*		_ASSERTE((GENINumber>=0)&&(GENINumber<=MAX_GENI_DEVICE_NUMBER)); */
		return(ErrorBadGENINumber);
	}
	pGENICardSetupData = &gefGENICardSetupData[GENINumber];
	Network = (GENINumber<<8);
	if ((Command<CommandOpenGENICard)||(Command>CommandWriteGENIIOPort)) {
/*		_ASSERTE ((Command>=CommandOpenGENICard)&&(Command<=CommandWriteGENIIOPort)); */
		return(ErrorBadGENICommand);
	}
/*
*	Get file handle for device, returning if not open and not an Open command
*/
	hDeviceHandle = pGENICardSetupData->GENIHandle;
	if ((Command!=CommandOpenGENICard) && !hDeviceHandle) {
/*		_ASSERTE((Command==CommandOpenGENICard) || hDeviceHandle); */
//		pGENICardSetupData = gefGENICardSetupData;
//		ByteLength = 0;
//		for (Network=0; Network<5; Network++) {
//			ByteLength += wsprintf(&SaveLine[ByteLength],"GENI=%u,Net=%d h=%d Port=%X\r\n",GENINumber,Network,hDeviceHandle,
//				pGENICardSetupData->IOPortBase);
//			pGENICardSetupData++;
//		}
//		SaveLine[ByteLength] = '\0';
//		MessageBox(NULL,SaveLine,"Open for GENI failed",MB_OK);
		return(ErrorGENINotOpen);
	}
	IOCTLStatus = 0;
	ReturnStatus = 0;
	cbReturned = 0;

	switch (Command) {
/*
//  The Open GENI/WSI command builds the file name, opens the device and saves
//  the handle for all other commands. It calls DeviceIOControl to return a 
//  pointer to the start of GENI or WSI shared memory. If it is a GENI card, >0,
//  that does not have both LED's on, it reinitializes the GENI card.
*/
	case CommandOpenGENICard:
#ifdef WIN32
/*
*	If Windows NT, create device file name and open to save handle
*/
		if (RunningWindowsNT) {
			strcpy(sFileName,"\\\\.\\GENI");
			strcat(sFileName,itoa(GENINumber,sDigits,10));
			hDeviceHandle = CreateFile(sFileName,GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,	NULL, 
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}
/*
*	If Windows 95, open the .VXD on the first call. FILE_FLAG_DELETE_ON_CLOSE
*	flag is used so that CloseHandle can be used to dynamically unload the VxD.
*	The FILE_FLAG_OVERLAPPED flag informs the Win32 subsystem that the VxD 
*	will be processing some DeviceIOControl calls asynchronously.
*/
		else {
			strcpy(sFileName,"\\\\.\\GENICard.VxD");
			if (!hVxD) {
				hVxD = CreateFile(sFileName, 0,0,0,0,FILE_FLAG_DELETE_ON_CLOSE, 0); 
/*                      FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OVERLAPPED, 0); */
/*				printf("FirstOpen=%d",hVxD); */
			}
			hDeviceHandle = hVxD;
		}
		if (hDeviceHandle == INVALID_HANDLE_VALUE) {
#ifdef TRACE_DISPLAY
			ReturnStatus = GetLastError();
			wsprintf(SaveLine,"\nGEF_AGH Not able to open device %s for number %u LastErr=%d",
					sFileName,GENINumber,ReturnStatus);
			MessageBox(NULL,SaveLine,"Failed Device Open",MB_OK|MB_SETFOREGROUND);
#endif
			pGENICardSetupData->GENIHandle = NULL;
			return(ErrorGENINotOpen);
		}
		else {

			IOCTLStatus = 1;
			pGENICardSetupData->GENIHandle = hDeviceHandle;
		}
#else
		pGENICardSetupData->GENIHandle = GENINumber;
#endif
//			wsprintf(SaveLine,"GENI=%u,h=%d Port=%X",GENINumber,hDeviceHandle,
//				pGENICardSetupData->IOPortBase);
//			MessageBox(NULL,SaveLine,"Open for GENI",MB_OK);
/*
//		WOWGetVDMPointerFix((LPVOID)MAKELONG(0,(WORD)pGENICardData->MemorySegment),16384,0);
//		if (!pGENICardData->pGENIMemory) {
//			MessageBox(NULL,"Failed to get 32 bit pointer to memory",
//			"Debug message",MB_SETFOREGROUND|MB_OK);
//			return(-1);
//		}
*/
		break;
/*
*   The Close command closes the file, which prevents any access until it is 
*   opened again. If pParameter is NULL or points to a 0, the GENI card is
*   reset to drop off the network, which turns off the LED's and causes all
*   Genius controlled devices to go to their default values. To leave the GENI
*	on so devices hold last state, set pParameter to address of a non-0 short
*	Have dropped code to turn off GENI, so pParameter1 ignored
*/
	case CommandCloseGENICard:
#ifdef WIN32
//		wsprintf(SaveLine,"GENI=%u,h=%d/%d Port=%X",GENINumber,pGENICardSetupData->GENIHandle,
//			hDeviceHandle,pGENICardSetupData->IOPortBase);
//		MessageBox(NULL,SaveLine,"Close for GENI",MB_OK);
		pGENICardSetupData->GENIHandle = NULL;
/*
*	If Win95 VxD, do not close single VxD handle until last GENIcard closed
*/
		if (hDeviceHandle) {
			if (RunningWindowsNT) {
				IOCTLStatus = CloseHandle(hDeviceHandle);
			}
			else {
				GENIStatusByte = 0;
				pGENICardSetupData = gefGENICardSetupData;
				for (Loop=0; Loop<=MAX_GENI_DEVICE_NUMBER; Loop++) {
					if (pGENICardSetupData->GENIHandle) {
						GENIStatusByte = 1;
						break;
					}
					pGENICardSetupData++;
				}
				if (!GENIStatusByte) {
					IOCTLStatus = CloseHandle(hDeviceHandle);
				}
			}
		}
#else
		pGENICardSetupData->GENIHandle = 0;
#endif
		break;

	case CommandTurnGENIOn:
	case CommandReadGENILED:
/*
*		The Read LED command returns a short word with the Port A LED bits in the
*		low byte. Bit 4 is off if PCIM OK is on and bit 5 is off if COMM OK is on
*/
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
            (GENICARD_READ_IO_PORT<<2)) : (GENICARD_READ_IO_PORT|Network));
		GENIStatusByte = 0;
		IOPortReadByte = 0;
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			&IOPortReadByte, sizeof(IOPortReadByte),
			&GENIStatusByte, sizeof(GENIStatusByte), &cbReturned, 0);
/*		printf("\nGEF_AGH Read GENI%u LED %X",GENINumber,GENIStatusByte); */
/*
*	Drop upper 3 bits of port data as may be 1. Also mask off interrupt bit. Low 2 bits
*	should be on and the bits 4 and 5 should be off. If only reading LED's, return
*/
		GENIStatusByte &= 0x1B;
		if (Command==CommandReadGENILED) {
			if (pParameter1) {
				*(WORD far *)pParameter1 = GENIStatusByte;
			}
			break;
		}
/*
*   Check both LEDs to see if this PCIM card is already on. If so, check the
*   broadcast data length. If card not on or different broadcast length, send
*   0x99 to PCIM Control port to turn it on. Then send initialization command
*   1 and 0x43 to command port and wait up to 3.5 seconds for card to turn on.
*   Check has to be limited to the low 5 bits as upper 3 bits may now be 1's
*   If PCIM card is not already on, sent commands to I/O port to turn on
*/
		if (GENIStatusByte != 3) {
			IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_WRITE_IO_PORT<<2)) : (GENICARD_WRITE_IO_PORT|Network));
			IOPortWriteByte = 0x9903;
			DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
			IOPortWriteByte = 0x0101;
			DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
			IOPortWriteByte = 0x4301;
			IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
/*			printf(" Turn GENI On"); */
		}
/*		else {
*			printf(" GENI already on");
*		}
*/
		break;

	case CommandTurnGENIOff:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_WRITE_IO_PORT<<2)) : (GENICARD_WRITE_IO_PORT|Network));
		IOPortWriteByte = 0x0101;
		DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
		IOPortWriteByte = 0x9903;
		DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
		IOPortWriteByte = 0x0101;
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortWriteByte,
				sizeof(IOPortWriteByte), NULL, 0, &cbReturned, 0);
/*		printf("\nGEF_AGH Turn off GENI Card %d",GENINumber); */
		break;
/*
*	Broadcast data length and Reference address from registry data (may add
*	option to override registry data).
*/
	case CommandChangeGENIConfig:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_CHANGE_CONFIG<<2)) : (GENICARD_CHANGE_CONFIG|Network));
		if (pParameter1) {
			pGENICardSetupData->PriorityGlobalByteLength = *(WORD far *)pParameter1;
			pGENICardSetupData->GlobalReference = *(((short far *)pParameter1)+1);
		}
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH ChangeConfig GENI%u,PriorityGlobalByteLength %X,Reference %u",
						GENINumber,pGENICardSetupData->PriorityGlobalByteLength,
						pGENICardSetupData->GlobalReference);
#endif
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			&pGENICardSetupData->PriorityGlobalByteLength, 4, NULL, 0, &cbReturned, 0);
		break;
/*
//  This is the recommended way to read and write data from the GENI memory.
//  The input must point to a ACCESS_GENI_MEMORY structure shown above with
//  the pointer to user application memory, the number of bytes to read or
//  write and the byte offset on the GENI shared memory (0 to 16383).
*/
	case CommandReadGENIMemory:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_READ_MEMORY<<2)) : (GENICARD_READ_MEMORY|Network));
		MemoryOffset = *(WORD far *)pParameter1;
		ByteLength = *(((WORD far *)pParameter1)+1);
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			&MemoryOffset, sizeof(WORD),pParameter2,ByteLength,&cbReturned, 0);
#ifdef TRACE_DISPLAY
/*		printf("\nGEF_AGH Read %d bytes from %X",ByteLength,MemoryOffset); */
#endif
		if (IOCTLStatus) {
			ReturnStatus = cbReturned;
		}
		break;
	case CommandWriteGENIMemory:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_WRITE_MEMORY<<2)) : (GENICARD_WRITE_MEMORY|Network));
		MemoryOffset = *(WORD far *)pParameter1;
		ByteLength = *(((WORD far *)pParameter1)+1);
/*		printf("\nGEF_AGH Write %d bytes from %X",ByteLength,MemoryOffset); */
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			pParameter1, ByteLength+4, NULL, 0, &cbReturned, 0);
		if (IOCTLStatus) {
			ReturnStatus = ByteLength;
		}
		break;
	case CommandTransferGENIMemory:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_TRANSFER_MEMORY<<2)) : (GENICARD_TRANSFER_MEMORY|Network));
/*		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, */
/*						 pParameter, 8, NULL, 0, &cbReturned, 0); */
		break;
	case CommandSendGENIDatagram:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_SEND_DATAGRAM<<2)) : (GENICARD_SEND_DATAGRAM|Network));
		if (pParameter1) {
			pGENICardMessage = (GENICARD_MESSAGE far *)pParameter1;
			ByteLength = pGENICardMessage->ByteLength + 6;
			IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
						 pParameter1, ByteLength, NULL, 0, &cbReturned, 0);
		}
		if (pParameter2) {
			ByteLength = sizeof(GENICARD_QUEUE);
			IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
						 NULL, 0, pParameter2, ByteLength, &cbReturned, 0);
		}
		break;
	case CommandReadGENIDatagram:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
				(GENICARD_READ_DATAGRAM<<2)) : (GENICARD_READ_DATAGRAM|Network));
		ByteLength = 0;
		if (pParameter1) {
			pGENICardMessage = (GENICARD_MESSAGE far *)pParameter1;
			ByteLength = pGENICardMessage->ByteLength + 6;
		}
		if (pParameter2) {
			IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
					pParameter1, ByteLength, pParameter2, 134, &cbReturned, 0);
		}
		break;
	case CommandDefineGENICallback:
/*
//		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, pParameter,
//					sizeof(GENIUS_INTERRUPT_MESSAGE), NULL, 0, &cbReturned,
//					(LPOVERLAPPED)&pGENICardData->BlockedEvent);
*/
/*
*   Need to set up wait for event, then PostThreadMessage back to caller
*   using GetWindowThreadProcessId if needed
*   Use ExInitializeWorkItem and ExQueueWorkItem to handle parameter,
*   then ExFreePool to release. None of this has been programmed yet.
*/
/*          ExInitializeWorkItem( ????? ) */
		break;
/*
*	Use to read a single byte from any I/O port defined for the device.
*	Input is 2 byte port offset from the base. A single data byte is returned
*	This is used for VDD support of old 16 bit DOS/Windows WSI or Genius
*/
	case CommandReadGENIIOPort:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_READ_IO_PORT<<2)) : (GENICARD_READ_IO_PORT|Network));
		if (pParameter1) {
			IOPortReadByte = *(WORD far *)pParameter1;
			if (pParameter2) {
				IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortReadByte,
					sizeof(WORD), pParameter2, sizeof(short), &cbReturned, 0);
#ifdef TRACE_DISPLAY
				printf("\nGEF_AGH Read GENI%u port %X = %X",GENINumber,IOPortReadByte,*(short *)pParameter2);
#endif
			}
			else {
				IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, &IOPortReadByte,
					sizeof(WORD), pParameter1, sizeof(short), &cbReturned, 0);
#ifdef TRACE_DISPLAY
				printf("\nGEF_AGH Read GENI%u port %X = %X",GENINumber,IOPortReadByte,*(short *)pParameter1);
#endif
			}
		}
		break;
/*
*	Use to write a single byte to any I/O port defined for the device.
*	Input is a byte port address offset from base followed by 1 byte of data
*	This is used for EEPROM access or VDD support of old 16 bit DOS/Windows WSI or Genius
*/
	case CommandWriteGENIIOPort:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_WRITE_IO_PORT<<2)) : (GENICARD_WRITE_IO_PORT|Network));
		if (pParameter1) {
			IOPortWriteByte = *(WORD far *)pParameter1;
			IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand, 
				&IOPortWriteByte, sizeof(WORD), NULL, 0, &cbReturned, 0);
#ifdef TRACE_DISPLAY
			printf("\nGEF_AGH Write GENI%u port %X = %X",GENINumber,IOPortWriteByte&0xFF,IOPortWriteByte>>8);
#endif
		}
		break;
	default:
/*		printf("\nGEF_AGH Unknown command %X",Command); */
		return(ErrorBadGENICommand);
	}
	if (!IOCTLStatus) {
#ifdef WIN32
		IOCTLStatus = GetLastError();
#endif
		ReturnStatus = -100;
	}
	return(ReturnStatus);
}
DllExport int WINAPI gefActivatePCIFRacks ( int StartingCardNumber, 
					int NumberOfCards, int CloseResetAction)
/*
Description

This procedure turns on (or activates) the number of Series 90-30 PCIF cards
specified by NumberOfCards starting with StartingCard, which is normally 1.

Return Value

++page
*/
{
	int LocalCard,Card,Rack,Slot,Status,Loop;
	int StatusSlowRead,StatusFastRead,StatusSlowWrite;
	int CardRackSlot,ByteCount,SlotCount;
	DWORD RackTypes;
	WORD WordsReturned[10];
	short PCIFCardStatus[MAX_PCIF_CARD_NUMBER];
/*	GENI_MEMORY_ACCESS Memory; */
	int CardsTurnedOn;
#ifdef TRACE_DISPLAY
	gefCommLibData.TraceDisplay = 0;
#endif
	LocalCard = StartingCardNumber; 
	if ((LocalCard<=0)||(LocalCard>MAX_PCIF_CARD_NUMBER)) {
		LocalCard = 1;
	}
	if ((NumberOfCards<=0)||(NumberOfCards>MAX_PCIF_CARD_NUMBER)) {
		NumberOfCards = MAX_PCIF_CARD_NUMBER + 1 - LocalCard;
	}
	LocalCard <<= 8;
/*
*   If CloseResetAction flag set, bit 1 is on for reset and/or bit 2 for close
*/
	if (CloseResetAction) {
		do {
			if (CloseResetAction&1) {
				gefAccessPCIFHardware(LocalCard, CommandTurnPCIFStop, NULL,NULL);
			}
			if (CloseResetAction&2) {
				gefAccessPCIFHardware(LocalCard, CommandClosePCIFCard, NULL,NULL);
			}
			LocalCard += 0x100;
		} while (--NumberOfCards>0);
		return(0);
	}

/*
*   Otherwise open/turn on devices from StartingDeviceNumber for number of GENI's
*/
	CardsTurnedOn = 0;
	for (Card=0; Card<NumberOfCards; Card++) {
		PCIFCardStatus[Card] = 0;
/*
*   Check if Network in range, will return with negative error code if not
*/
/*		Network = gefActivateCommLibrary ((LocalDevice+Loop), gefCommTypeGENI, -1); */
/*		WideStatus = -1; */
		if (gefAccessPCIFHardware(LocalCard,CommandOpenPCIFCard,NULL,(WORD *)&RackTypes)>0) {;
/*printf("[Return Racks=%X]",RackTypes); */
			PCIFCardStatus[Card] = 1;
/*
			if (gefAccessPCIFHardware(Network,CommandTurnPCIFRun,NULL,NULL)>=0) {
				PCIFCardStatus[Loop] = 2;
			}
*/
			ByteCount = 0;
/*			for (Rack=1; Rack<8; Rack++) { */
			for (Rack=1; Rack<=4; Rack++) {
				CardRackSlot = LocalCard | (Rack<<4);
				SlotCount = 10;
				for (Slot=0; Slot<=SlotCount; Slot++) {
/*
*	Slot 0, check slot 10 for module
*/
					if (!Slot) {
#ifdef TRACE_DISPLAY
						if (Rack<=3) {
							gefCommLibData.TraceDisplay = 1;
						}
#endif
/*
*	Do slow read/write to get ModuleID, then wait 50 MS for response
*/
						gefAccessPCIFHardware(CardRackSlot|0x8A,CommandReadPCIFData,
								(WORD *)&ByteCount,WordsReturned);
						gefAccessPCIFHardware(CardRackSlot|0x8A,CommandWritePCIFData,
								(WORD *)&ByteCount,WordsReturned);
#ifdef WIN32
						Sleep(50);
#endif
						for (Loop=0; Loop<2; Loop++) {
							StatusSlowRead = (gefAccessPCIFHardware(CardRackSlot|0x8A,CommandReadPCIFData,
								(WORD *)&ByteCount,WordsReturned)>>8)&0x1F;
#ifdef TRACE_DISPLAY
							if (gefCommLibData.TraceDisplay) printf(" SlowRead");
#endif
							StatusSlowWrite = (gefAccessPCIFHardware(CardRackSlot|0x8A,CommandWritePCIFData,
								(WORD *)&ByteCount,WordsReturned)>>8)&0x1F;
#ifdef TRACE_DISPLAY
							if (gefCommLibData.TraceDisplay) printf(" SlowWrite");
#endif
							if (StatusSlowRead&&StatusSlowWrite) {
								break;
							}
						}
						StatusFastRead = (gefAccessPCIFHardware(CardRackSlot|0x0A,CommandReadPCIFData,
								(WORD *)&ByteCount,WordsReturned)>>8)&0x1F;
#ifdef TRACE_DISPLAY
						if (gefCommLibData.TraceDisplay) printf(" FastRead");
						printf("\nRack %u Slow=%X,Fast=%X,Write=%X",Rack,
							StatusSlowRead,StatusFastRead,StatusSlowWrite);
if (gefCommLibData.TraceDisplay) {
	printf(" Pause, press key");
	_getche();
}
						gefCommLibData.TraceDisplay = 0;
#endif
/*
*	Skip rack if slow read of slot 10 is 0. If fast read failed, set slow rate
*/
						if (!StatusSlowRead) {
							gefPCIFRemoteModuleID[Card][Rack][Slot] = 0;
							break;
						}
						Status = 2;
						if (StatusSlowRead!=StatusSlowWrite) {
							SlotCount = 5;
							Status = 1;
						}
						if (!StatusFastRead) {
							CardRackSlot |= 0x80;
							Status += 2;
						}
						gefPCIFRemoteModuleID[Card][Rack][Slot] = (BYTE)Status;
#ifdef TRACE_DISPLAY
						printf("[PCIF%X=%X]",CardRackSlot,Status);
#endif
						PCIFCardStatus[Card] |= 1<<Rack;
					}
					else {
/*
*	Step to next slot and read 2 words to get Module ID. Use data rate for rack
*/
						CardRackSlot++;
						for (Loop=0; Loop<5; Loop++) {
							Status = gefAccessPCIFHardware(CardRackSlot,CommandReadPCIFData,
								(WORD *)&ByteCount,WordsReturned);
							Status >>= 8;
							if (!(Status&0x80)) {
								break;
							}
#ifdef TRACE_DISPLAY
							printf("[Busy=%X]",Status);
#endif
						}
						Status &= 0x1F;
						if ((Status>0)&&(Status<0x1F)) {
							if (CardRackSlot&0x80) {
								Status |= 0x80;
							}
#ifdef TRACE_DISPLAY
							printf("[PCIF%X=%X]",CardRackSlot,Status);
#endif
						}
						else {
							Status = 0;
						}
						gefPCIFRemoteModuleID[Card][Rack][Slot] = (BYTE)Status;
					}
				}
			}
		}
/*
*	If any racks found for card, increment card count
*/
		if (PCIFCardStatus[Card]&0xFFE) {
			CardsTurnedOn++;
		}
		LocalCard += 0x100;
	}
#ifdef TRACE_DISPLAY
	gefCommLibData.TraceDisplay = 1;
#endif
	return(CardsTurnedOn);
}
DllExport int WINAPI gefAccessPCIFHardware(long PCIFCardRackSlot,
			int Command, WORD far *pTypeByteLength,	WORD far *pTransferData)
{
	HANDLE  hDeviceHandle;
	int PCIFCard;
	int   IOCTLCommand,ByteLength,Network,cbReturned;
	WORD    WordsSent[130],WordsReturned[129];
	BOOL    IOCTLStatus;
	int ReturnStatus;
	static int CompletedFirstCall=0;
	static int RunningWindowsNT; 
#ifdef WIN32
	char sFileName[20],sDigits[18];
	OSVERSIONINFO OSVersionInfo;
#endif
	PCIFCARD_SETUP_DATA *pPCIFCardSetupData;

	if (!CompletedFirstCall) {
		CompletedFirstCall = 1;
		memset(gefPCIFCardSetupData,0,sizeof(gefPCIFCardSetupData));
		memset(gefPCIFRemoteModuleID,0,sizeof(gefPCIFRemoteModuleID));
/*
*	Determine the operating system. The Win 32 v4.0 choices are	VER_PLATFORM_WIN32s,
*	VER_PLATFORM_WIN32_WINDOWS, or VER_PLATFORM_WIN32_NT
*/
		RunningWindowsNT = FALSE;
#ifdef WIN32
		OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&OSVersionInfo);
		if (OSVersionInfo.dwPlatformId==VER_PLATFORM_WIN32_NT) {
			RunningWindowsNT = TRUE;
		}
#endif
	}
	PCIFCard = PCIFCardRackSlot>>8;
	IOCTLStatus = 0;
	ReturnStatus = 0;
	cbReturned = 0;
/*  Check that Command and GENINumber, (1 to 16 for GENI) are in range */
	if ((PCIFCard<=0)||(PCIFCard>MAX_GENI_DEVICE_NUMBER)) {
/*		_ASSERTE((PCIFCard>0)&&(PCIFCard<=MAX_GENI_DEVICE_NUMBER)); */
		return(ErrorBadGENINumber);
	}
	pPCIFCardSetupData = &gefPCIFCardSetupData[PCIFCard-1];
	Network = ((PCIFCard+16)<<8);
	if ((Command<CommandOpenGENICard)||(Command>CommandWriteGENIIOPort)) {
/*		_ASSERTE ((Command>=CommandOpenGENICard)&&(Command<=CommandWriteGENIIOPort)); */
		return(ErrorBadGENICommand);
	}
/*
*	Get device file handle. Return success if Opening already Open or
*	error if any other operation on file that is not open.
*/
	hDeviceHandle = pPCIFCardSetupData->PCIFHandle;
	if (Command==CommandOpenGENICard) {
		if (hDeviceHandle) {
			return(0);
		}
	}
	else {
		if (!hDeviceHandle) {
/*		_ASSERTE((Command==CommandOpenGENICard) || hDeviceHandle); */
			return(ErrorGENINotOpen);
		}
	}

	switch (Command) {
/*
*	The Open PCIF command builds the file name, opens the device and saves
*	the handle for all other commands.
*/
	case CommandOpenPCIFCard:
#ifdef WIN32
/*
*	If Windows NT, create device file name and open to save handle
*/
		if (RunningWindowsNT) {
			strcpy(sFileName,"\\\\.\\PCIF");
			strcat(sFileName,itoa(PCIFCard,sDigits,10));
			hDeviceHandle = CreateFile(sFileName,GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,	NULL, 
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}
/*
*	If Windows 95, open the .VXD on the first call. FILE_FLAG_DELETE_ON_CLOSE
*	flag is used so that CloseHandle can be used to dynamically unload the VxD.
*	The FILE_FLAG_OVERLAPPED flag informs the Win32 subsystem that the VxD 
*	will be processing some DeviceIOControl calls asynchronously.
*/
		else {
			strcpy(sFileName,"\\\\.\\GENICard.VxD");
			if (!hVxD) {
				hVxD = CreateFile(sFileName, 0,0,0,0, FILE_FLAG_DELETE_ON_CLOSE, 0); 
/*                      FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OVERLAPPED, 0); */
/*				printf("FirstOpen=%d",hVxD); */
			}
			hDeviceHandle = hVxD;
			CountOpenVxD++;
		}
		if (hDeviceHandle == INVALID_HANDLE_VALUE) {
			printf("\nGEF_AGH Not able to open device %s for number %u",sFileName,PCIFCard);
			pPCIFCardSetupData->PCIFHandle = NULL;
			return(ErrorGENINotOpen);
		}
		else {
			pPCIFCardSetupData->PCIFHandle = hDeviceHandle;
			IOCTLStatus = 1;
		}
#else
		pPCIFCardSetupData->PCIFHandle = PCIFCard;
#endif
		break;
/*
*   The Close command closes the file, which prevents any access until it is 
*   opened again. If pParameter is NULL or points to a 0, the GENI card is
*   reset to drop off the network, which turns off the LED's and causes all
*   Genius controlled devices to go to their default values. To leave the GENI
*	on so devices hold last state, set pParameter to address of a non-0 short
*	Have dropped code to turn off GENI, so pParameter1 ignored
*/
	case CommandClosePCIFCard:
#ifdef WIN32
		pPCIFCardSetupData->PCIFHandle = NULL;
/*
*   If Win95 VxD, do not close single VxD handle until last GENI/PCIF closed
*/
		if (hDeviceHandle) {
			if (RunningWindowsNT) {
				IOCTLStatus = CloseHandle(hDeviceHandle);
			}
			else {
				if (!(--CountOpenVxD)) {
					IOCTLStatus = CloseHandle(hDeviceHandle);
				}
			}
		}
#else
		pPCIFCardSetupData->PCIFHandle = 0;
#endif
		break;

	case CommandGetPCIFState:
	case CommandTurnPCIFRun:
	case CommandTurnPCIFStop:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_READ_PCIF_DATA<<2)) : (GENICARD_READ_PCIF_DATA|Network));
		WordsSent[0] = (WORD)((PCIFCardRackSlot&0xF00)|(Command-CommandGetPCIFState));
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, 2, WordsReturned, 2, &cbReturned, 0);
		ReturnStatus = WordsReturned[0];
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Set PCIF%u to %X Returned %X",PCIFCard,WordsSent[0],WordsReturned[0]);
#endif
		break;

	case CommandSetPCIFWatchDog:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_SET_WATCHDOG<<2)) : (GENICARD_SET_WATCHDOG|Network));
		WordsSent[0] = (WORD)(PCIFCardRackSlot&0xF00);
		WordsSent[1] = 0;
		if (pTypeByteLength) {
			WordsSent[1] = *(WORD far *)pTypeByteLength;
		}
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, 4, &ReturnStatus, 4, &cbReturned, 0);
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Set PCIF%u watchdog to %X",PCIFCard,WordsSent[1]);
#endif
		break;

	case CommandReadPCIFData:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_READ_PCIF_DATA<<2)) : (GENICARD_READ_PCIF_DATA|Network));
		WordsSent[0] = (WORD)PCIFCardRackSlot;
		WordsSent[1] = 2;
		if (pTypeByteLength) {
			WordsSent[1] = *pTypeByteLength;
		}
		ByteLength = (WordsSent[1]&0xFF) + 2;
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, 4, WordsReturned, ByteLength, &cbReturned, 0);
		ReturnStatus = WordsReturned[0];
		if (pTransferData&&(cbReturned>2)) {
			memcpy(pTransferData,&WordsReturned[1],cbReturned-2);
		}
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Read PCIF%Xh sent %u bytes Returned %u bytes %X ",PCIFCardRackSlot,
			ByteLength,cbReturned,WordsReturned[0]);
#endif
		break;

	case CommandWritePCIFData:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_WRITE_PCIF_DATA<<2)) : (GENICARD_WRITE_PCIF_DATA|Network));
		WordsSent[0] = (WORD)PCIFCardRackSlot;
		WordsSent[1] = 2;
		if (pTypeByteLength) {
			WordsSent[1] = *pTypeByteLength;
		}
		ByteLength = WordsSent[1]&0xFF;
		if (pTransferData) {
			memcpy(&WordsSent[2],pTransferData,ByteLength);
		}
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, ByteLength+4, WordsReturned, 2, &cbReturned, 0);
		ReturnStatus = WordsReturned[0];
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Write PCIF%Xh %u bytes Returned %X ",PCIFCardRackSlot,WordsSent[1],WordsReturned[0]);
#endif
		break;

	case CommandReadPCIFSmartData:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_READ_PCIF_DMA<<2)) : (GENICARD_READ_PCIF_DMA|Network));
		WordsSent[0] = (WORD)PCIFCardRackSlot;
		WordsSent[1] = 2;
		if (pTypeByteLength) {
			WordsSent[1] = *pTypeByteLength;
		}
		ByteLength = (WordsSent[1]&0xFF) + 2;
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, 4, WordsReturned, ByteLength, &cbReturned, 0);
		ReturnStatus = WordsReturned[0];
		if (pTransferData&&(cbReturned>2)) {
			memcpy(pTransferData,&WordsReturned[1],cbReturned-2);
		}
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Read PCIF%Xh DMA %X Cmd/bytes Returned %X",PCIFCardRackSlot,WordsSent[1],WordsReturned[0]);
#endif
		break;

	case CommandWritePCIFSmartData:
		IOCTLCommand = (RunningWindowsNT ? (IOCTL_GENIUS_BASE_FUNCTION |
			(GENICARD_WRITE_PCIF_DMA<<2)) : (GENICARD_WRITE_PCIF_DMA|Network));
		WordsSent[0] = (WORD)PCIFCardRackSlot;
		WordsSent[1] = 2;
		if (pTypeByteLength) {
			WordsSent[1] = *pTypeByteLength;
		}
		ByteLength = WordsSent[1]&0xFF;
		if (pTransferData) {
			memcpy(&WordsSent[2],pTransferData,ByteLength);
		}
		IOCTLStatus = DeviceIoControl (hDeviceHandle, IOCTLCommand,
			WordsSent, ByteLength+4, WordsReturned, 2, &cbReturned, 0);
		ReturnStatus = WordsReturned[0];
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Write PCIF%Xh DMA %X Cmd/bytes Returned %X",PCIFCardRackSlot,WordsSent[1],WordsReturned[0]);
#endif
		break;

	default:
#ifdef TRACE_DISPLAY
		printf("\nGEF_AGH Unknown command %X for PCIF%Xh",Command,PCIFCardRackSlot);
#endif
		return(ErrorBadGENICommand);
	}
	if (!IOCTLStatus) {
#ifdef WIN32
		IOCTLStatus = GetLastError();
#endif
		ReturnStatus = -100;
	}
	return(ReturnStatus);
}
typedef struct {
	WORD    CardRemoteBitRackSlot;/* Card=0=F, add 0x80 if remote rack */
	WORD    ModuleIDByteLength;	/* ModID in high, even byte length in lo */
	BYTE	*pIOData;
} TRANSFER_30IO_DATA;
typedef struct {
	WORD	OutputInputBytes;	/* output byte length in high, input low */
	char	*pTypeLabel;
} IOMODULE_INFO;
const IOMODULE_INFO gef30IOModuleInfo[32] = {0,"NoRack",0,"PowerSupply",0,"CPU",
	0,"Local5",0,"Local10",0,"Remote5",0,"Remote10", /* added */
	0x008,"AI4",0,"8?",0,"9?",0,"10?",0x400,"AQ2",0,"12?",0x202,"QI16",
	0x404,"QI32",0x004,"I32",0x800,"Q64",0x008,"I64",0x800,"AQ4",0,"IOM",
	0,"20?",0,"IOHS",0x002,"IODMA",0x002,"I16",0x101,"QI8",0,"FOIM",
	0x404,"QI32",0x200,"Q16",0x808,"QI64",0x001,"I8",0x100,"Q8",0,"NoModule"};
DllImport int WINAPI gefTransfer30IOModules(
							int	WriteData,		/* Read=FALSE/0,Write=TRUE/1 */
							int ModuleCount,	/* modules in struct */
							TRANSFER_30IO_DATA *pTransferIO);
#define MAX_MODULE_LIST 200
GEF_MODULE_LIST LocalModuleList[MAX_MODULE_LIST];
int ModuleCount;
DllExport DWORD WINAPI gefElapsedMilliSec(int fInitialize, DWORD *pMilliSec)

{
	DWORD CurrentMilliSec;

#ifdef WIN32
	CurrentMilliSec = timeGetTime();
#else
/*	This does NOT handle the day rollover, so reinitialize */
	DWORD far *pDOSClockWord = (DWORD far *) 0x0040006CL;
	CurrentMilliSec = *pDOSClockWord*55;
#endif
	if (pMilliSec) {
		if (fInitialize) {
			*pMilliSec = CurrentMilliSec;
			CurrentMilliSec = 0L;
		}
		else {
#ifndef WIN32
			if (CurrentMilliSec<*pMilliSec) {
				*pMilliSec = CurrentMilliSec;
			}
#endif
			CurrentMilliSec -= *pMilliSec;

		}
	}
	return(CurrentMilliSec);
}
#ifdef DEFINED_IN_GEF_ACLN
DllExport WORD WINAPI gefTimeDelay(WORD Milliseconds)
{
#ifdef WIN32
	SYSTEMTIME SystemTime;
	Sleep(Milliseconds);
	GetSystemTime(&SystemTime);
	return(SystemTime.wMilliseconds/55);
#else
	volatile WORD far *pDOSClockLowWord = (WORD far *) 0x0040006CL;
	return(*pDOSClockLowWord);
#endif
}
#endif
DllExport int WINAPI gefRead30IOConfig(
							int PCIFCard,
							int Rack1to7,
							int Slot1to10,
							int MaxConfigByteCount,
							MODULE_CONFIG_30IO far *pConfig30IO,
							BYTE far *pConfigData)
{
	return(0);
}
GEF_MODULE_LIST *gefLocate30IOModule(int CardRackSlot)
{
	GEF_MODULE_LIST *pModuleList;
/*	GEF_PRODUCT_LIST *pProductList; */
	int Loop,Status;
	WORD LocalCardRackSlot,ModuleID;
	WORD ByteLength,ReturnData[2];
	const IOMODULE_INFO *pIOModuleInfo;
	static int FirstCall=1;

	if (FirstCall) {
		FirstCall = 0;
		memset(LocalModuleList,0,sizeof(LocalModuleList));
		ModuleCount = 0;
	}
/*
*	Check if this card/rack/slot identified. Hi bit 0x8000 set for PCIF
*/
	pModuleList = LocalModuleList;
	LocalCardRackSlot = (WORD)(CardRackSlot|0x8000);
	if (ModuleCount>0) {
		for (Loop=0; Loop<ModuleCount; Loop++) {
			if (LocalCardRackSlot==pModuleList->CardRackSlot) {
				LocalCardRackSlot = 0;
				break;
			}
			pModuleList++;
		}
	}
/*
*	If not found, add to end with PCIF hi bit set if still room
*/
	if (LocalCardRackSlot&&(ModuleCount<MAX_MODULE_LIST)) {
		pModuleList->CardRackSlot = LocalCardRackSlot;
		ModuleCount++;
	}
/*
*	If hi byte with Module ID still 0, read word to get real ID
*/
	if (!(pModuleList->FoundConfigModuleID&0xFF00)) {
		ModuleID = 0;
/*
		LocationLength[0] = (WORD) (CardRackSlot - 0x101);
		LocationLength[1] = 2;
		Status = gefTestIOCTLProcessing(GENICARD_READ_PCIF_DATA,
			(BYTE *)LocationLength,4,(BYTE *)ReturnData,sizeof(ReturnData),pBytes);
*/
		ByteLength = 2;
		Status = gefAccessPCIFHardware(CardRackSlot,CommandReadPCIFData,
					&ByteLength, ReturnData);

		pModuleList->FoundConfigModuleID |= (WORD)(ReturnData[0]&0xFF00);
printf("[Loc %X ID=%X/",CardRackSlot,ReturnData[0]);
		ModuleID = ReturnData[0]>>8;
		if ((ModuleID>0)&&(ModuleID<31)) {
			pIOModuleInfo = &gef30IOModuleInfo[ModuleID];
			if (!pModuleList->WordByteInLength) {
				pModuleList->WordByteInLength = pIOModuleInfo->OutputInputBytes&0xFF;
			}
			if (!pModuleList->WordByteOutLength) {
				pModuleList->WordByteOutLength = pIOModuleInfo->OutputInputBytes>>8;
			}
printf("%s=%uR/%uW]\n",pIOModuleInfo->pTypeLabel,pModuleList->WordByteInLength,
				pModuleList->WordByteOutLength);
		}
/*
*   Need to search product list to determine bytes in/out from module ID
*/
	}
	return(pModuleList);
}
DllExport int WINAPI gefRead30IOModule(int Card, int Rack, int Slot,
							short far *pData)
{
	int Status,Command,ByteLength,ModuleID,CardRackSlot;
	const IOMODULE_INFO *pIOModuleInfo;
	ModuleID = 0;
	Status = 0;
	if ((Card>0)&&(Card<=MAX_PCIF_CARD_NUMBER)&&(Slot>0)&&(Slot<=10)) {
		ModuleID = gefPCIFRemoteModuleID[Card-1][Rack&7][Slot];

		pIOModuleInfo = &gef30IOModuleInfo[ModuleID];
		ByteLength = pIOModuleInfo->OutputInputBytes&0xFF;
		CardRackSlot = (Card<<8)|((Rack&0xF)<<4)|Slot;
		if (ModuleID&0x80) {
			ModuleID &= 0x1F;
			CardRackSlot |= 0x80;
		}
/*		printf("[ID=%x Loc=%x,%u bytesR",ModuleID,CardRackSlot,ByteLength); */
		if (ModuleID==0x16) {
			ByteLength |= 0xE000;
			Command = CommandReadPCIFSmartData;
		}
		else {
			Command = CommandReadPCIFData;
			if (ByteLength>8) {
				printf("Error byte length %u too big, cut to 8",ByteLength);
				ByteLength = 8;
			}
		}
printf("[R3M%X@%X=%u bytes]",ModuleID,CardRackSlot,ByteLength);
		Status = gefAccessPCIFHardware(CardRackSlot,Command,
			(WORD *)&ByteLength, (WORD far *)pData);
	}
	return(Status);
}
DllExport int WINAPI gefStore30IOConfigX(
							int CardRackSlot,
							int ConfigByteOffset,
							int ConfigByteCount,
							BYTE *pConfigData)
{
	printf("\nStore30IOConfig not written yet");
	return(0);
}
DllExport int WINAPI gefTransfer30IOModules(
							int	WriteData,		/* Read=FALSE/0,Write=TRUE/1 */
							int ModuleCount,	/* modules in struct */
							TRANSFER_30IO_DATA *pTransferIO)
{
	int Loop,ByteLength,TotalBytesRead,TotalBytesWrite;
	GEF_MODULE_LIST *pModuleList;
	WORD LocalBuffer[129];

	TotalBytesRead = 0;
	TotalBytesWrite = 0;
	for (Loop=0; Loop<ModuleCount; Loop++) {
/*
*   Get model ID type and byte length based on direction if still 0
*/
		if (!pTransferIO->ModuleIDByteLength) {
			pModuleList = gefLocate30IOModule((int)pTransferIO->CardRemoteBitRackSlot);
			if (WriteData) {
				ByteLength = pModuleList->WordByteOutLength&0xFF;
				if (pModuleList->WordByteOutLength&0xFF00) {
					ByteLength += (pModuleList->WordByteOutLength>>7)&0xFE;
					if (ByteLength>=256) {
						ByteLength = 255;
					}
				}
			}
			else {
				ByteLength = pModuleList->WordByteInLength&0xFF;
				if (pModuleList->WordByteInLength&0xFF00) {
					ByteLength += (pModuleList->WordByteInLength>>7)&0xFE;
					if (ByteLength>=256) {
						ByteLength = 255;
					}
				}
			}
			pTransferIO->ModuleIDByteLength = (WORD)((pModuleList->FoundConfigModuleID&0xFF) | ByteLength);
		}
		if (WriteData) {
		}
		else {
			LocalBuffer[Loop] = pTransferIO->CardRemoteBitRackSlot;
		}
#ifdef OLD_COMMENTS
	short   CardRackSlot;   /* 3 hex digits, Card=1-F, Rack=1-7, Slot=1-A */
	short   ModuleTypeByteCount;/* init 0, sets module type and length */
	BYTE    *pIOData;   /* data to write or where to return data read */
} TRANSFER_30IO_DATA;
#endif
		pModuleList++;
	}
	if (WriteData) {
	}
	else {
	}
	return(0);
}
DllExport int WINAPI gefWrite30IOModule(int Card, int Rack, int Slot,
							short far *pData)
{
	int Status,Command,ModuleID,CardRackSlot;
	WORD TempWords[2],ByteLength;
/*    WORD CardRackSlotLength[2],ReturnData[2]; */
	const IOMODULE_INFO *pIOModuleInfo;
	ModuleID = 0;
	Status = 0;
	if ((Card>0)&&(Card<=MAX_PCIF_CARD_NUMBER)&&(Slot>0)&&(Slot<=10)) {
		ModuleID = gefPCIFRemoteModuleID[Card-1][Rack&7][Slot];
		pIOModuleInfo = &gef30IOModuleInfo[ModuleID];
		ByteLength = (WORD)(pIOModuleInfo->OutputInputBytes>>8);
		CardRackSlot = (Card<<8)|((Rack&0xF)<<4)|Slot;
		if (ModuleID&0x80) {
			ModuleID &= 0x1F;
			CardRackSlot |= 0x80;
		}
/*		printf("[Loc=%X,%u bytesW",CardRackSlot,ByteLength); */
/*
		DataLength = 4;
		if (pData) {
			if (ByteLength>0) {
				DataLength += ByteLength;
			}
		}
*/
		if (ModuleID==0x16) {
			ByteLength |= 0xE100;
			Command = CommandWritePCIFSmartData;
			Status = gefAccessPCIFHardware(CardRackSlot,Command,
				&ByteLength,(WORD far *)pData);
		}
		else {
			Command = CommandWritePCIFData;
			if (ByteLength>8) {
				printf("Error byte length %u too big, cut to 8",ByteLength);
				ByteLength = 8;
			}
			if ((ModuleID==0x0B)||(ModuleID==0x1E)) {
				ByteLength = 2;
				if (ModuleID==0x0B) {
/*
*   Special processing for 2 channel analog output sent as 2 separate
*	channel writes of 2 bytes each of 13 bit sign magnitude number in
*	left end and low 3 bits of 2 for channel 1 and 1 for channel 2
*   Write channel 2 out first, then use normal code to write channel 1
*/
					memcpy(TempWords,pData,4);
					if (TempWords[0]&0x8000) {
						TempWords[0] = -((short)TempWords[0]) | 0x8000;
					}
					TempWords[0] = (TempWords[0]&0xFFF8)|0x2;
					if (TempWords[1]&0x8000) {
						TempWords[1] = -((short)TempWords[1]) | 0x8000;
					}
					TempWords[1] = (TempWords[1]&0xFFF8)|0x1;
					gefAccessPCIFHardware(CardRackSlot,Command,
							&ByteLength,&TempWords[1]);
				}
/* Old PCIFVIEW
				if ((ByteLength&0xF)==4) {
					pDataWord = (short far *)pData;
					Length = 2;
					do {
						if (*pDataWord<0) {
							*pDataWord = -(*pDataWord) | 0x8000;
						}
						*pDataWord++ = (*pDataWord&0xFFF8) + Length;
					} while (--Length>0);
					ByteLength = (ByteLength&0xFFF0) | 2;
					gefAccessPCIFModule(PortRackSlot, ByteLength, &Data[2]);
				}
*/
				else {
/*
*   Special processing for 8 bit discrete output where some modules
*	place 8 data bits in low byte and others in high byte. Copy to both
*/
					TempWords[0] = (WORD)(*pData&0xFF);
					TempWords[0] |= TempWords[0]<<8;
				}
				pData = (short *)TempWords;
			}
printf("[W3M%X@%X=%u bytes]",ModuleID,CardRackSlot,ByteLength);
			Status = gefAccessPCIFHardware(CardRackSlot,Command,
				&ByteLength,(WORD far *)pData);
		}
	}
	return(Status);
}

