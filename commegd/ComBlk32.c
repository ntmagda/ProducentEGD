/*
*	This is code to emulate the 16-bit Micro/Sys Drumlin CommBLOK library
*	under 32-bit Windows. This allows the Genius to Ethernet gateway code
*	to support RTU serial communications under Windows as well as on
*	a Micro/Sys Netsock/100 micro controller with PC-104 GENI cards
*/
#include <windows.h>
#include <stdio.h>
#include "CommEGD.h"
static HANDLE hSerialPorts[4];	/* static location for COM1 to COM4 handles */
int cb_hwsetup(int COMPort, WORD Port, int IRQ, int SizeTX, int SizeRX)
/*
*	Hardware setup not required for Windows. Buffer size set to 8 KB for SNP below
*/
{
	return(0);
}
CCB *cb_open(int COMPort, long DataRate, char Parity, int StopBits, int DataBits,
	int DTRHigh, int RTSHigh, int Mode)
/*
*	Open serial port under Windows and return pointer to Handle that has been saved
*	Return a NULL pointer if unable to open the COM port
*/
{
	int ReceiveBufferSize,Status,Connected;
	HANDLE hSerial;
	char COMPortName[10];
    COMMTIMEOUTS CommTimeOuts;
	DCB dcb;

	if ((COMPort<=0)||(COMPort>4)) {
		return(NULL);
	}
	memcpy(COMPortName,"COM1",5);
	COMPortName[3] = (char)(COMPort + '0');
	hSerial = CreateFile(COMPortName, GENERIC_READ|GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
	if (hSerial==INVALID_HANDLE_VALUE) {
		Status = GetLastError();
		printf("\nOpen %s return handle %d failed, Last Error %u",COMPortName,hSerial,Status);
		return(NULL);
	}
	hSerialPorts[COMPort-1] = hSerial;
// setup device input and output buffers to 8 KB for maximum SNP length and purge 
	ReceiveBufferSize = 8192;
	Status = SetupComm( hSerial, ReceiveBufferSize,ReceiveBufferSize ) ;
	Status = PurgeComm( hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
// set up -1 for overlapped I/O which also ignores other times. Use 0 for normal
	CommTimeOuts.ReadIntervalTimeout = 0;	/* 0 worked direct */
//	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF ;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
//	short   ModemTurnTime;  /* Modem Turnaround time in milliseconds */
	CommTimeOuts.ReadTotalTimeoutConstant = 0 ;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = 1000 ;
	Status = SetCommTimeouts( hSerial, &CommTimeOuts ) ;

	dcb.DCBlength = sizeof( DCB ) ;
	Status = GetCommState( hSerial, &dcb ) ;
/*
*	Set default data rate and bits/parity to 19200/8Odd if below 300/7None
*/
	dcb.BaudRate = (DWORD) DataRate;
	dcb.ByteSize = 8;
	if (Parity) {		
		dcb.Parity = (BYTE) ((Parity==2) ? EVENPARITY : ODDPARITY);
		dcb.fParity = TRUE;
	}
	else {
		dcb.Parity = NOPARITY;
		dcb.fParity = FALSE;
	}
	dcb.StopBits = ONESTOPBIT;
	if (Parity>2) {
		dcb.StopBits = TWOSTOPBITS;
	}
/*
*	Set up flow control, with default of none for PLC protocols
*/
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = (DTRHigh ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE);
//	if (pNetwork->Data.SNP.DataBitsParity&gefSetCommStateMaskFlowDTR) {
//		dcb.fOutxDsrFlow = TRUE;
//		dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
//	}
	dcb.fOutxCtsFlow = FALSE;
	dcb.fRtsControl = (RTSHigh ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE);
//	if (pNetwork->Data.SNP.DataBitsParity&gefSetCommStateMaskFlowDTR) {
//		dcb.fOutxCtsFlow = TRUE;
//		dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
//	}
	dcb.fInX = FALSE;
	dcb.fOutX = FALSE;
	dcb.XonChar = 0x11;			// Default X-On character is Control Q or DC1
	dcb.XoffChar = 0x13;		// Default X-Off character is Control S or DC3
	dcb.XonLim = ReceiveBufferSize/2;	// 50% of receive buffer free
	dcb.XoffLim = ReceiveBufferSize/10;	// 10% of buffer left
	dcb.fBinary = TRUE;
	dcb.fNull = FALSE;
	dcb.fErrorChar = FALSE;
	dcb.fAbortOnError = TRUE;
	Connected = SetCommState( hSerial, &dcb );

	return(&hSerialPorts[COMPort-1]);
}
char *cb_getblock(char *pString, int ByteCount, CCB *pSerial)
/*
*	Write a block of binary data and wait for completion as not overlapped
*/
{
	int Status,ByteCountRead;
	DWORD dwErrorFlags;
	COMSTAT ComStat;
/*
*   Just clear buffers and return if the byte count is 0
*/
	if (ByteCount<=0) {
		PurgeComm(*pSerial, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR ) ;
		return(0);
	}
	ClearCommError(*pSerial, &dwErrorFlags, &ComStat ) ;
	Status = ReadFile(*pSerial,pString,ByteCount,&ByteCountRead,NULL);
	return(pString);
}
char *cb_putblock(char *pString, int ByteCount, CCB *pSerial)
/*
*	Read block of binary data. Always call cb_rxcib to determine how many bytes
*	are in the input buffer so that program will never have to wait
*/
{
	int Status,ByteCountRead;
	DWORD dwErrorFlags;
	COMSTAT ComStat;

/*
*   Just clear buffers and return if the byte count is 0
*/
	if (ByteCount<=0) {
		PurgeComm(*pSerial, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR ) ;
		return(0);
	}
	ClearCommError(*pSerial, &dwErrorFlags, &ComStat ) ;
	Status = WriteFile(*pSerial,pString,ByteCount,&ByteCountRead,NULL);
	return(pString);
}
int cb_rxcib(CCB *pSerial)
/*
*	Return bytes in the receive buffer
*/
{
	DWORD ErrorCodes;
	COMSTAT CommStatus;

	ClearCommError(*pSerial, &ErrorCodes, &CommStatus);
	return((int)CommStatus.cbInQue);
}
int cb_txcib(CCB *pSerial)
/*
*	Return bytes in transmit buffer. Should be 0 under Windows as not using the
*	overlapped writes
*/
{
	DWORD ErrorCodes;
	COMSTAT CommStatus;

	ClearCommError(*pSerial, &ErrorCodes, &CommStatus);
	return((int)CommStatus.cbOutQue);
}
int cb_clrbreak(CCB *pSerial)
/*
*	Clear the break signal used for SNP/SNP-X protocols
*/
{
	ClearCommBreak(*pSerial);
	return(0);
}
int cb_setbreak(CCB *pSerial)
/*
*	Set the break signal used for SNP/SNP-X protocols. Wait 10 to 50 MS before clearing
*/
{
	PurgeComm(*pSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
	SetCommBreak(*pSerial);
	return(0);
}
