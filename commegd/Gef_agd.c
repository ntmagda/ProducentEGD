#ifdef WIN32
#include <windows.h>
#else
int Sleep(int Delay);
long GetTickCount(void);
#endif
#include "gefcomm.h"
#define GENI_DATA_STORAGE 1
#include "genilib.h"
/*#include "genilibx.h" */
#include <string.h>
#include <stdio.h>	/* for printf */
short gefGENISerialBusAddress[16];
/*
*	The Driver check routine below has been moved from gef_acln.c which
*	is being converted to gef_cplc.c
*/
int gefCheckGEFCommDriver(void)
/*
*	Routine called by the GEFComm dll or other programs not using the DLL
*	to check for drivers and start GENICard
*/
{
    HANDLE      hEvent = 0;
    OVERLAPPED  ovlp = {0,0,0,0,0};
	char DriverName[_MAX_PATH],*pDriverName;
	FILETIME CreationDateDriver,CreationDateFile;
	HANDLE  hDeviceHandle,hFile;
	DWORD   Status;
	int RunningWindowsNT;
	char SaveLine[200];
	OSVERSIONINFO OSVersionInfo;
	SC_HANDLE hServiceControl,hGENICardService;
    SERVICE_STATUS ServiceStatus;

/*
*	Determine the operating system. The Win 32 v4.0 choices are	VER_PLATFORM_WIN32s,
*	VER_PLATFORM_WIN32_WINDOWS, or VER_PLATFORM_WIN32_NT
*/
	OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&OSVersionInfo);
	if (OSVersionInfo.dwPlatformId==VER_PLATFORM_WIN32_NT) {
		RunningWindowsNT = TRUE;
		pDriverName = getenv("SystemRoot");
		if (!pDriverName) {
			return(0);
		}
		strcpy(DriverName,pDriverName);
		strcat(DriverName,"\\system32\\drivers\\GENICard.sys");
	}
	else {
		RunningWindowsNT = FALSE;
		pDriverName = getenv("WINDIR");
		if (!pDriverName) {
			return(0);
		}
		strcpy(DriverName,pDriverName);
		strcat(DriverName,"\\system\\GENICard.VxD");
	}
//	Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, gefGENICardRegistryKey, 0,KEY_ALL_ACCESS,
//													&hGENICard); 
/*
*	Get date for installed driver file and the same file in PATH(current directory)
*/	
	hDeviceHandle = CreateFile(DriverName,GENERIC_READ,0, NULL, 
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDeviceHandle!=INVALID_HANDLE_VALUE) {
		GetFileTime(hDeviceHandle,NULL,NULL,&CreationDateDriver);
	}
	else {
		hDeviceHandle = 0;
	}
	pDriverName = strrchr(DriverName,'\\') + 1;
	hFile = CreateFile(pDriverName,GENERIC_READ,0, NULL, 
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile!=INVALID_HANDLE_VALUE) {
		GetFileTime(hFile,NULL,NULL,&CreationDateFile);
	}
	else {
		hFile = 0;
	}
/*
*	If driver file has equal or newer date last written, ignore other file, 
*/
	if (hDeviceHandle && hFile) {
		if (CompareFileTime(&CreationDateDriver,&CreationDateFile)>=0) {
			CloseHandle(hFile);
			hFile = 0;
		}
	}
/*
*	Close both files in case they must be copied below
*/
	if (hDeviceHandle) {
		CloseHandle(hDeviceHandle);
	}
	if (hFile) {
		CloseHandle(hFile);
	}
	if (hFile) {
		if (hDeviceHandle) {
			wsprintf(SaveLine,"Driver file %s is out of date.\n\rDo you want to update it?",DriverName);
		}
		else {
			wsprintf(SaveLine,"Driver file %s does not exist.\n\rDo you want to install it?",DriverName);
		}
		Status = MessageBox(NULL,SaveLine,"Old or Missing GENICard Device Driver",MB_YESNO|MB_ICONQUESTION);
		if (Status==IDYES) {
			CopyFile(pDriverName,DriverName,FALSE);
		}
	}
/*
*	If running NT, check if GENICard service is running. If not, start it.
*/
	if (RunningWindowsNT) {
		hServiceControl = OpenSCManager(NULL, NULL,SC_MANAGER_ALL_ACCESS);
		if (hServiceControl) {
			hGENICardService = OpenService(hServiceControl,"GENICard",SERVICE_ALL_ACCESS);
			if (hGENICardService) {
				QueryServiceStatus(hGENICardService, &ServiceStatus);
				if (ServiceStatus.dwCurrentState!=SERVICE_RUNNING) {
					if (!StartService(hGENICardService, 0, NULL)) {
						MessageBox(NULL,"Unable to Start GENICard Service","CheckGEFCommDriver Failed",MB_OK|MB_SETFOREGROUND);
					}
				}
				CloseServiceHandle(hGENICardService);
			}
			else {
				MessageBox(NULL,"Unable to Open GENICard Service","CheckGEFCommDriver Failed",MB_OK|MB_SETFOREGROUND);
			}
			CloseServiceHandle(hServiceControl);
		}
		else {
			MessageBox(NULL,"Unable to Open Service manager","CheckGEFCommDriver Failed",MB_OK|MB_SETFOREGROUND);
		}
	}
	return(RunningWindowsNT);
}
DllExport int WINAPI gefActivateGENIDevices ( int StartingDeviceNumber, 
					int NumberOfDevices, int CloseResetAction)
/*
C       gefActivateGENIDevices ( int StartingDeviceNumber, int NumberOfDevices,
								int CloseResetAction)

BASIC   gefActivateGENIDevices% ( StartingDeviceNumber%, NumberOfDevices%, )

Pascal  gefActivateGENIDevices ( StartingDevice, NumberOfDevices:integer):integer;

Description

This procedure turns on (or activates) the number of Genius network PCIM cards 
specified by NumberOfDevices starting with StartingDevice, which is normally 1.
It is the 32 bit Windows NT/95 replacement for the 16 bit gefActivateGENICard.
All setup data for the GENI devices must already be defined in the registry.
The function returns a negative number for an error or a positive number of
GENI cards turned on if it is successful. If NumberOfDevices is 1, the
function return value is the Genius Serial Bus Address of the GENI card for
compatibility with gefActivateGENICard. If NumberOfDevices is 0, it just
returns the Genius SBA for the StartingDevice GENI card. This procedure must 
be called before other GEFCOMM procedures can access the PCIM cards.
The CloseResetAction is 0 to turn the GENI cards on. Set bit 1 on to turn
GENI cards off or bit 2 to close GENI cards(use 3 for both turn off and close)
Many other GEFCOMM procedures will use the Device number as the first
parameter to identify which PCIM card, or Genius network, is being accessed.


The MemorySegment can be set to 0 to turn the specified PCIM card off.

This routine toggles the watchdog timer bit on the PCIM card during its delay
loops, so the watch dog timer jumper can be enabled (JP2 on motherboard set
to position 2-3). The application must continue pulsing the watchdog timer
bit at least every 727 milliseconds, or the PCIM card will shut down.

Return Value

The routine returns the PCIM device number (0 to 31) if the card starts up,
or the negative GENI failure status if it does not start.

GENISuccess or larger   Card turned on, or off if MemorySegment = 0
GENIErrorBadAddress(-8) Network number invalid, limited from 1 to 9
GENIErrorBadComm(-9)    Network/cable problem, PCIM COMM OK LED turned off
GENIErrorBadPort(-10)   IOPortAddress invalid/duplicate, check SW1 on PCIM
GENIErrorBadMemory(-11) MemorySegment invalid/duplicate, check PCIM SW2/SW3
GENIErrorHeartbeatOff(-12) GENI memory problem, exclude 16 KB from memory mgr.

Related Functions

gefActivateSerialPort, gefChangeGENIConfig, gefCheckGENIStatus
++page
*/
{
	int LocalDevice,Loop,Network, CountCardsStillOff,Heartbeat;
	long WideStatus;
	DWORD StartTickCount,CurrentTickCount,DeltaTickCount;
	short GENICardStatus[MAX_GENI_DEVICE_NUMBER];
	GENI_MEMORY_ACCESS Memory;
	int PCIMStatusByte,SerialBusAddress,CardsTurnedOn;
//	char SaveLine[100];

	LocalDevice = StartingDeviceNumber; 
	if ((LocalDevice<=0)||(LocalDevice>MAX_GENI_DEVICE_NUMBER)) {
		LocalDevice = 1;
	}
	if ((NumberOfDevices<=0)||(NumberOfDevices>MAX_GENI_DEVICE_NUMBER)) {
		NumberOfDevices = MAX_GENI_DEVICE_NUMBER + 1 - LocalDevice;
	}
/*
*   If CloseResetAction flag set, bit 1 is on for reset and/or bit 2 for close
*/
	if (CloseResetAction) {
		do {
			if (CloseResetAction&1) {
				gefAccessGENIHardware(LocalDevice, CommandTurnGENIOff, NULL,NULL);
			}
			if (CloseResetAction&2) {
				gefAccessGENIHardware(LocalDevice, CommandCloseGENICard, NULL,NULL);
			}
			LocalDevice++;
		} while (--NumberOfDevices>0);
		return(0);
	}

/*
*   Otherwise open/turn on devices from StartingDeviceNumber for number of GENI's
*/
	CardsTurnedOn = 0;
	for (Loop=0; Loop<NumberOfDevices; Loop++) {
		GENICardStatus[Loop] = 0;
/*
*   Check if Network in range, will return with negative error code if not
*/
/*		Network = gefActivateCommLibrary ((LocalDevice+Loop), gefCommTypeGENI, -1); */
		Network = LocalDevice + Loop;
//		wsprintf(SaveLine,"GENI%d of %d to %d",Network,Loop,NumberOfDevices);
//		MessageBox(NULL,SaveLine,"Turning ON",MB_OK);
		WideStatus = -1;
		if (Network>0) {
/*			pNetwork = gefNetworkData; */
/*			pNetwork += Network; */
			if (gefAccessGENIHardware(Network,CommandOpenGENICard,NULL,NULL)>=0) {;
				GENICardStatus[Loop] = 1;
				if (gefAccessGENIHardware(Network,CommandTurnGENIOn,NULL,NULL)>=0) {
					GENICardStatus[Loop] = 2;
				}
			}
		}
	}
/*
*	Read LED's for all cards to see how many are still off or starting up
*/
	CountCardsStillOff = 0;
	for (Loop=0; Loop<NumberOfDevices; Loop++) {
		Network = LocalDevice + Loop;
		if (GENICardStatus[Loop]) {
			gefAccessGENIHardware(Network, CommandReadGENILED, &PCIMStatusByte,NULL);
/*			printf("\nNetwork %u has LEDs=%X",Network,PCIMStatusByte); */
			PCIMStatusByte &= 0x1B;
/*
*	If both LED's on, check heartbeat. If on, read DIPswitch for SBA number
*/
			if (PCIMStatusByte==3) {
				Memory.Offset = 0x893;
				Memory.Length = 1;
				Heartbeat = 0;
				gefAccessGENIHardware(Network, CommandReadGENIMemory,
							&Memory, &Heartbeat);
				if (Heartbeat==1) {
					Memory.Offset = 0x882;
					Memory.Length = 1;
					SerialBusAddress = -1;
					gefAccessGENIHardware(Network, CommandReadGENIMemory,
							&Memory, &SerialBusAddress);
					gefGENISerialBusAddress[Network-1] = (short)(SerialBusAddress&0x1F);
					GENICardStatus[Loop] = 5;
					CardsTurnedOn++;
				}
			}
			else {
				CountCardsStillOff++;
			}
		}
	}
/*
*	If all GENI cards are already on, return
*/
	if (!CountCardsStillOff) {
		return(CardsTurnedOn);
	}
/*
*	Now loop through all card that had to be turned on. When their heartbeat
*	is seen after 2 seconds, send change GENI config command. 
*/
	StartTickCount = GetTickCount();
	CurrentTickCount = 0;
	do {
		Sleep(55);
		CurrentTickCount = GetTickCount();
		if (CurrentTickCount<StartTickCount) {
			StartTickCount = ~(StartTickCount-1);
		}
		DeltaTickCount = CurrentTickCount - StartTickCount;
		for (Loop=0; Loop<NumberOfDevices; Loop++) {
			Network = LocalDevice + Loop;
			if (GENICardStatus[Loop]) {
				gefAccessGENIHardware(Network, CommandReadGENILED, &PCIMStatusByte,NULL);
				PCIMStatusByte &= 0x1B;
/*				printf("\nGENI%u Read LED %X",Network,PCIMStatusByte); */
				if ((DeltaTickCount>2000)&&(GENICardStatus[Loop]<3)) {
					Memory.Offset = 0x893;
					Memory.Length = 1;
					Heartbeat = 0;
					gefAccessGENIHardware(Network, CommandReadGENIMemory,
							&Memory, &Heartbeat);
					if (Heartbeat==1) {
						gefAccessGENIHardware(Network, CommandChangeGENIConfig, NULL,NULL);
/*						printf("\nChangeGENIConfig from GEF_AGD"); */
						GENICardStatus[Loop] = 3;
					}
				}
				if ((DeltaTickCount>2500)&&(GENICardStatus[Loop]==3)) {
					Memory.Offset = 0x893;
					Memory.Length = 1;
					Heartbeat = 0;
					gefAccessGENIHardware(Network, CommandReadGENIMemory,
							&Memory, &Heartbeat);
					if (Heartbeat==1) {
						Memory.Offset = 0x882;
						Memory.Length = 1;
						SerialBusAddress = -1;
						gefAccessGENIHardware(Network, CommandReadGENIMemory,
							&Memory, &SerialBusAddress);
						gefGENISerialBusAddress[Network-1] = (short)(SerialBusAddress&0x1F);
						GENICardStatus[Loop] = 4;
/*
*	Disable all 7 types of interrupts
*/
						Memory.Offset = 0x8A2;
						Memory.Length = 32;
						memset(Memory.Data,0,Memory.Length);
						memset(&Memory.Data[16],1,7);
						WideStatus = gefAccessGENIHardware(Network, CommandWriteGENIMemory,
								&Memory, NULL);
						if (WideStatus>=0) {
							GENICardStatus[Loop] = 5;
							CardsTurnedOn++;
						}
					}
				}
			}
		}
	} while (DeltaTickCount<3500);
	return(CardsTurnedOn);
}
#ifdef OLD_AGC_STUFF
		if (WideStatus>=0) {
/*
*   Check both LEDs to see if this PCIM card is already on. If so, check the
*   broadcast data length. If card not on or different broadcast length, send
*   0x99 to PCIM Control port to turn it on. Then send initialization command
*   1 and 0x43 to command port and wait up to 3.5 seconds for card to turn on.
*   Check has to be limited to the low 5 bits as upper 3 bits may now be 1's
*/
			PulseWatchDog = 0;
			TickCountLimit = 3500/55;
			TickCount = 2000/55;
/*
*   If PCIM card is not already on, sent commands to I/O port to turn on
*/
			gefAccessGENIHardware(Network, CommandReadGENILED, &PCIMStatusByte,NULL);
			printf("\nNetwork %u has LEDs=%X",Network,PCIMStatusByte); 
			Loop = 5;
	do {
		Status = 
		printf("\nLoop %u has LEDs=%X, Heartbeat=%X, Scan %d Rev %X",PCIMStatusByte,
 						pPCIM->Heartbeat,pPCIM->BusScanTime,pPCIM->RevisionNumber); 
		getche();
	} while (--Loop>0);

	if (PCIMStatusByte != 3) {
/*
*   Pulse the watchdog every 110 MS in case watchdog jumper JP2 enabled.
*/
		LastDOSTick = gefTimeDelay(0);
		for (TickCount=0; TickCount<TickCountLimit; TickCount++) {
			do {
				CurrentDOSTick = gefTimeDelay(0);
			} while (CurrentDOSTick==LastDOSTick);
			LastDOSTick = CurrentDOSTick;
			if (TickCount&1) {
				OutPortAddressData = 0x4201;
				if (PulseWatchDog&1) {
					OutPortAddressData |= 0x100;
				}
				gefAccessGENIHardware(Network, CommandWriteGENIIOPort,
											&OutPortAddressData,NULL);
				PulseWatchDog++;
			}
/*
*   Quit as soon as the PCIM Heartbeat comes on after 2.0 sec. The PCIM_OK
*   LED will still be off until the configuration change is complete
*/
			if (TickCount>(2000/55)) {
				gefAccessGENIHardware(Network, CommandReadGENILED,
						&PCIMStatusByte,NULL);
				if (pPCIM->Heartbeat==1) {
					printf("\nHeartbeat = 1, press key");
					getche();
					break;
				}
			}
		}
	}
/*
*   Check if the broadcast data length has been changed from the initialization
*   values of 0. The Series 90/70 requires this to be set correctly within
*   the 1.5 second configuration window after the PCIM turns on, or the PLC
*   will log a configuration mismatch. Always do it to shorten startup time.
*/
	if (PulseWatchDog) {
		Status = gefChangeGENIConfig(Network,pNetwork->Data.Geni.InputLength,
			pNetwork->Data.Geni.OutputLength, pNetwork->Data.Geni.ReferenceAddress);
		if (Status<GENISuccess) {
			pNetwork->FailureStatus = Status;
			return(Status);
		}
	}
/*
*   If card started up or configuration length has changed, wait for the rest
*   of the 3.5 seconds for both LEDs to turn on. Continue watchdog pulsing
*   Wait one extra DOS tick, but other devices may not have had time to log in
*/
	gefAccessGENIHardware(Network, CommandReadGENILED, &PCIMStatusByte,NULL);
	if (PulseWatchDog) {
		LastDOSTick = gefTimeDelay(0);
		do {
			do {
				CurrentDOSTick = gefTimeDelay(0);
			} while (CurrentDOSTick==LastDOSTick);
			LastDOSTick = CurrentDOSTick;
			if (TickCount&1) {
				OutPortAddressData = (0x42 + (PulseWatchDog&1))<<16 + PCIMPort + 1;
				gefAccessGENIHardware(Network, CommandWriteGENIIOPort,
											&OutPortAddressData, NULL);
				PulseWatchDog++;
			}
			if (!(PCIMStatusByte&0x18)) {
				break;
			}
			gefAccessGENIHardware(Network, CommandReadGENILED, &PCIMStatusByte, NULL);
		} while (++TickCount<TickCountLimit);
	}
/*
*   Check if GENI_OK Heartbeat signal and PCIM LEDs are on. If not, set the
*   Address(was Segment) to 0 so procedures will not access this card and return bad data.
*/
	Status = GENISuccess;
	if (pPCIM->Heartbeat!=1) {
		Status = GENIErrorHeartbeatOff;
	}
	if (PCIMStatusByte&0x08) {
		Status = GENIErrorBadPort;
	}
	if (PCIMStatusByte&0x10) {
		Status = GENIErrorBadComm;
	}
	pNetwork->LastCommFailureStatus = Status;
	if (Status<GENISuccess) {
		pNetwork->Data.GENI.pGENIBaseAddress = NULL;
		return(Status);
	}
/*
*   Disable all PCIM interrupts and clear status. User can set them up.
*   There are 8 bytes, so clear or set them as 4 double bytes;
*/
	pPCIM = NULL;	/* DEBUG */
	if (pPCIM) {
		pFarWord = (short far *)pPCIM->InterruptStatus;
		memset(pFarWord,0,32);
		pFarWord = (short far *)pPCIM->InterruptDisable;
		memset(pFarWord,1,7);
/*
*   This PCIM network card is running OK, Return its device number(0 to 31)
*/
		Status = (pPCIM->GENIDipSwitch&0x1F);
		printf("\nPCIM SBA is %u, press key",Status);
		getche();
	}
/*
*   Most code below may get moved into the kernel driver as part of the
*   CommandReadGENIMemory, CommandWriteGENIMemory and CommandTransferGENIMemory 
*   DeviceIOControl commands. This will eliminate user access to GENI shared
*   memory entirely, like Microsoft recommends for a more secure driver.
*/
int CIMLIB gefGENIQuickMove( int ReadAction, GENI_MEMORY far *pPCIM,
					int GENIOffset, int ByteCount, BYTE far *pDataWords)
/*
C       int gefGENIQuickMove ( int ReadAction, GENI_MEMORY far *pPCIM,
						int GENIOffset,int ByteCount, BYTE far *pDataWords)

BASIC   gefGENIQuickMove% ( ReadAction%, SEG pPCIM AS GENIMEMORY, GENIOffset%,
						ByteCount%, SEG DataWords AS ANY)

Pascal  gefGENIQuickMove ( ReadAction, var pPCIM, GENIOffset,
						ByteCount:integer, DataWords: pointer): integer

Description

Procedure to move multiple words from PCIM memory with GENI lockout, but
without any overhead for error checking. It is intended for internal use by
the GENILIB routines, and is not for general use.

Return Value

The procedure returns a positive byte count if the transfer is successful or
a 0 if the GENI is busy. If a 0 is returned, the procedure MUST be called
again until it is successful.

Related Functions

gefGetGENIDatagram, gefSendGENIDatagram, gefReadGENIDevice, gefWriteGENIDevice
*/

{
	BYTE far *pPCIMByte;
	int Status = 0;          /* set status assuming GENI is busy */
	int Count;

	if (ByteCount<=0) {
		return(1);
	}
/*
*   Return if the Lockout Request and Lockout State flags are equal. If not,
*   we are in the middle or requesting (or releasing) the GENI daughtercard
*   memory control. We MUST call this procedure again to retry.
*   Convert memory segment to a far pointer, without using library routines
*/
	if (pPCIM->IOTableLockoutRequest == pPCIM->IOTableLockoutState) {
/*
*   If Lockout Request and State are the same and are = 1, the request to
*   control the GENImemory has been granted. Transfer DataWords
*   from (if ReadAction is true) or to the Offset location in the PCIM card.
*   Set Lockout request back to 0 at end.
*/
		if (pPCIM->IOTableLockoutRequest) {
			pPCIMByte = (BYTE far *) pPCIM;
			pPCIMByte += GENIOffset;
			Count = ByteCount;
			if (ReadAction) {
/*
				_fmemmove((char far *)pDataWords, pPCIMByte, ByteCount);
*/
				do {
					*pDataWords++ = *pPCIMByte++;
				} while (--Count>0);
			}
			else {
/*
				_fmemmove(pPCIMByte, (char far *)pDataWords, ByteCount);
*/
				do {
					*pPCIMByte++ = *pDataWords++;
				} while (--Count>0);
			}
			pPCIM->IOTableLockoutRequest = 0;
			Status = ByteCount;
		}
		else {
/*
*   If Lockout Request and State are the same and are = 0, the GENI is not
*   using the memory, but we need to request it by setting the Request
*   to a 1. This procedure MUST be called again to complete the data transfer.
*/
			pPCIM->IOTableLockoutRequest = 1;
		}
	}
	return(Status);
}

#endif
