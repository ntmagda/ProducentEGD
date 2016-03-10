/*
*	This is part of the Genius to Ethernet gateway that includes
*	MODBUS RTU and UDP (which is TCP messages using UPD datagrams)
*	The target platform for GENIGate is a Micro/Sys Netsock/100
*	25 MHz 80188 PC-104 controller with 2 COM ports and Ethernet UDP
*	support in the BIOS. It will support GE Fanuc Ethernet Global Data
*	(EGD) and MODBUS/UDP on Ethernet, one or more PC-104 Genius bus
*	controllers and COM ports for RTU slave (and possibly RTU master)
*
*	Serial comm uses the Drumlin CommBLOCK library that is also sold
*	by Micro/Sys at www.embeddedsys.com or (818) 244-4600, Montrose, CA
*	The comm routines are mapped to the Win32 serial API for unbundled
*	operation on a standard PC or other computer platform
*/	 
//#define FAST_CRC	/* use to switch to fast CRC calculations using table lookup
typedef unsigned long DWORD;                 
typedef unsigned short WORD;                 
typedef unsigned char BYTE;
#include "CommEGD.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
//static BYTE SerialBuffer[300];
/*	
*	Internal memory storage for 16384 I/Q's, 8192 AI/AQ's and M's and R's 
*	The R's and M's are for internal memory. Any length can be changed.
*	You can modify the pMemoryTableStart to point anywhere as long as you
*	update the MemoryTableLength to match as there are used by transfer code
*/
static BYTE I[2048],Q[2048],M[2048];
static short AI[8192],AQ[8192],R[8192];
static const BYTE *pMemoryTableStart[6] = {I,Q,(BYTE *)AI,(BYTE *)AQ,M,(BYTE *)R};
static const int MemoryTableLength[6] = {
	sizeof(I),sizeof(Q),sizeof(AI),sizeof(AQ),sizeof(M),sizeof(R)};
enum {MODBUSMemoryI=0,MODBUSMemoryQ,MODBUSMemoryAI,MODBUSMemoryAQ,MODBUSMemoryM,MODBUSMemoryR};
#ifndef GEF_GATEWAY
//	Standalone test program that is not part of the GE Fanuc gateway
int main (int argc, char *argv[])
{
	int SlaveID,Parity,COMPort,Loop,Letter,Value,CharacterTimeoutMS;
	long DataRate;                                       
	char Slave[8];
	CCB *pSlave;
	static const char *pParityLabels[6]={"None/1","Odd/1","Even/1","None/2","Odd/2","Even/2"};
	
	COMPort = 1;
	DataRate = 19200;
	Parity = 1;
	SlaveID = 1;
/*
*	Check input parameters for new setup. First parameter character
*	is letter (COM1 to COM4 or Odd/Even) or digit for ID or DataRate
*/
	if (argc>1) {    
		for (Loop=1; Loop<argc; Loop++) {
			Letter = argv[Loop][0];
			if (Letter>='a') {
				Letter &= 0x5F;
			}                                   
			if (Letter=='C') {
				Value = argv[Loop][3] - '0';
				if ((Value>0)&&(Value<=4)) {
					COMPort = Value;
				}
			}
			else {
				if ((Letter>='0')&&(Letter<='9')) {
					Value = atoi(argv[Loop]);
					if (Value<300) {
						SlaveID = Value;
					}
					else {
						DataRate = Value;
					}
				}
				else {            
/*
*	Check commands None, Odd or Even for parity or Master to set slave ID to 0
*/
					if (Letter=='N') {
						Parity = 0;
					}
					if (Letter=='O') {
						Parity = 1;
					}
					if (Letter=='E') {
						Parity = 2;
					}
					if (Letter=='M') {
						SlaveID = 0;
					}
				}
			}            
		}
	}                      
	memcpy(Slave,"Master",7);
	if (SlaveID>0) {
		itoa(SlaveID,Slave,10);
	}           
	printf("Program command line parameters are:  Gateway\t%s COM%u %u %s\nEnter a Control C to stop the test program.\n",
		Slave,COMPort,(WORD)DataRate,pParityLabels[Parity]);
	pSlave = gefSetupSerialPort(COMPort, DataRate, Parity);    
	CharacterTimeoutMS = (int)(40000L/DataRate);
/*
*	Fill internal memory with dummy data for testing. Assume word and bit memory same length
*/	
	for (Loop=1; Loop<=sizeof(I); Loop++) {
		I[Loop-1] = (BYTE)Loop;
		Q[Loop-1] = (BYTE)(Loop|0x80);
	}
	for (Loop=1; Loop<=(sizeof(AI)>>1); Loop++) {
		AI[Loop-1] = Loop;
		AQ[Loop-1] = Loop | 0x2000;
		R[Loop-1] = Loop | 0x4000;
	}
/*
*	Will check for Ethernet and RTU serial communications continuously
*	until a Control C is entered on the keyboard
*/
	while (1) { 
		if (kbhit()) {
			if (getche()==3) {
				break;
			}
		}                                       
		if (SlaveID>0) {
			gefProcessSlaveRTU(pSlave, SlaveID,CharacterTimeoutMS);
		}
#ifdef WIN32
		Sleep(10);
#endif
	}		
	return(0);
}
#endif
CCB *gefSetupSerialPort(int COMPort, long DataRate, int ParityStopBits)
/*
*	Routine to set up serial communications for binary PLC protocols.
*	This code is for the 16-bit Drumlin CommBLOK library for PC's or
*	micro controllers
*/
{   
	int Error,StopBits;
	char Parity;
	CCB *pControlBlock;
	static WORD IOAddress[4]={0x3F8,0x2F8,0x3E8,0x2E8};
	static int IRQ[4]={4,3,4,3};
	static char ParityLetter[3] = {'N','O','E'};
/*
*	Setup Netsock/100 Ethernet microcontroller with non-standard COM ports
*/
//	cb_setcputype(SBC1190);  	                                 
	if ((COMPort<=0)||(COMPort>4)) {
		COMPort = 1;
	}
	Error = cb_hwsetup(COMPort,IOAddress[COMPort-1],IRQ[COMPort-1],1024,1024);
//		sizeof(SerialBuffer),sizeof(SerialBuffer));
	pControlBlock = (CCB *)NULL;
	if (!Error) {      
/*
*	Convert 0 to 5 for None/1, Odd/1, Even/1, None/2, Odd/2 Even/2 to 
*	values used for Drumlin CommBLOK library
*/
		StopBits = 1;
		Parity = 'N';
		if (ParityStopBits>0) {
			if (ParityStopBits>2) {
				ParityStopBits -= 3;
			}
			if (ParityStopBits<3) {
				Parity = ParityLetter[ParityStopBits];
			}
		}
		pControlBlock = cb_open(COMPort,DataRate,Parity,StopBits,8,0,1,PTP_MODE|CTS_RTS_CTRL);
	}               
	return(pControlBlock);
}                                       
int gefCalculateCRChecksum ( int ByteCount, BYTE far *pBuffer)
/*
*   The gefCalculateCRChecksum routine calculates the CRC-16 or CRC-CCITT
*   checksum for ByteCount bytes in pBuffer array and returns the 2 byte checksum.
*
*   The program may use static WORD CRCTable[256] to hold CRC values calculated
*   on the first call, The CharPoly value is set to the characteristic polynomial
*   used to initialize a 256 word table pointed to by pCRCTable. Use a CharPoly
*   of 0x8408 for a CRC-CCITT polynomial or 0xA001 for a CRC-16 polynomial.
*	The fast CRC lookup does not seem to work so it has ben replaced by in-line code
*/
{
	int Count;
	int Shift;
	WORD CheckSum;
#ifdef FAST_CRC_HAS_NOT_BEEN_DEBUGGED
	static WORD CRC16Table[256]=0;
	WORD far *pCRCTable,CharPoly;
	BYTE Offset;
/*
*   If first table value is 0, build the 256 word CRC table in pCRCTable.
*   The table built is CRC-CCITT polynomial (0x8408), for CRC-16 use (0xA001)
*/  
	pCRCTable = CRC16Table;
	if (!CRC16Table[0]) {
		CharPoly = 0xA001;
		Count = 0;
		do {
			CheckSum = Count++;
			Shift = 8;
			while (Shift--) {
				CheckSum = (CheckSum >> 1) ^ ((CheckSum & 1) ? CharPoly : 0);
			}
			*pCRCTable++ = CheckSum;
		} while (Count<256);
	}
/*
*   Calculate the checksum for array pBuffer using the pCRCTable calculated above.
*	CODE GAVE WRONG ANSWER, SO I RECODED BELOW
*/
	CheckSum = 0;
	Count = ByteCount;
	do {
		printf("[%X]",*pBuffer);
		Offset = (BYTE) (CheckSum ^ ((short) *pBuffer++));
		CheckSum = *(pCRCTable + Offset) ^ (CheckSum >> 8);
	} while (--Count>0);                                 
#else
	Count = ByteCount;
	CheckSum = 0xFFFF;
	do {
		CheckSum ^= *pBuffer++;
		Shift = 8;
		do {
			if (CheckSum&1) {
				CheckSum >>= 1;
				CheckSum ^= 0xA001;
			}
			else {
				CheckSum >>= 1;
			}
		} while (--Shift>0);
	} while (--Count>0);
#endif
	return(CheckSum);
 }
int gefProcessSlaveRTU(CCB *pSerialPort, int SlaveID, int CharacterTimeoutMS)
/*
*	RTU Slave routine to process RTU messages from a remote master.
*	Data is retrieved from to store to local I%, IQ, %AI, %AQ or %R
*	memory. This routine only processes complete messages
*/
{
	int BytesIn,BytesOut,DataBytesTransferred,ExpectedByteLength;
	int Station,Function,ResponseLength,DeltaMS;
	static BYTE SerialBuffer[300];
	static int BytesToProcess=0;         
	clock_t CurrentTime;
	static clock_t LastTime=0;
	GEF_MODBUS_RTU_REQUEST *pRequest;
/*
*	Return if transmitting data or nothing in receive queue
*	Reset input buffer being processed if character timeout
*/                 
	BytesIn = cb_rxcib(pSerialPort);
	BytesOut = cb_txcib(pSerialPort);
	if ((BytesOut>0)||(BytesIn<=0)) {
		return(0);
	}	
/*
*	Check time passed sine last characters. If too long, drop message
*	THIS IS TO DROP PARTIAL MESSAGES OR ONES WITH GAPS. NOT DONE
*	NEED TO USE THE HIGHEST RESOLUTION CLOCK AS GAP IS 2 MS at 19200
*/
	CurrentTime = clock();
	DeltaMS = (int)((1000*(CurrentTime - LastTime))/CLOCKS_PER_SEC);
	if (DeltaMS>CharacterTimeoutMS) {
		BytesToProcess = 0;
	}
	LastTime = CurrentTime;
/*
*	Add new input data to buffer, adjusting start to fit(error condition)
*/ 
	if ((BytesToProcess+BytesIn)>sizeof(SerialBuffer)) {
		BytesToProcess = sizeof(SerialBuffer) - BytesIn;
	}
	cb_getblock(&SerialBuffer[BytesToProcess],BytesIn,pSerialPort);
//printf("Add %u to Buff[%u]",BytesIn,BytesToProcess);
	BytesToProcess  += BytesIn;
	if (BytesToProcess<4) {
		return(0);
	}
/*
*	If at least 4 byte (exception reques) check if complete message
*/
	pRequest = (GEF_MODBUS_RTU_REQUEST *)SerialBuffer;         
	Station = pRequest->UnitIdentifier;
	Function = pRequest->MODBUSFunctionCode;	
/*
*	Determine expected byte length based on the function code and data length
*/
	ExpectedByteLength = 8;
	if ((Function==7)||(Function==17)) {
		ExpectedByteLength = 4;
	}                                                                       
/*
*	If writing multiple values, get byte length, which may include extended length 
*/
	if ((Function==15)||(Function==16)) {
		ExpectedByteLength = 259;
		if (BytesToProcess>=9) {
			if (SerialBuffer[6]==0xFF) {
				ExpectedByteLength = (SerialBuffer[7]<<8) + SerialBuffer[8] + 9;
			}
			else {
				ExpectedByteLength = SerialBuffer[6] + 9;
			}      
		}
	}
/*
*	If message not complete, return as still more data to receive
*/
	if (BytesToProcess<ExpectedByteLength) {
		return(0);
	}
/*
*	If complete message for a different station, drop it and return. 
*/
	if ((Station>247)||((Station>0)&&(Station!=SlaveID))) {
		BytesToProcess = 0;
printf("\nIgnored, Wrong Station %u",Station);
		return(0);
	}
	if (BytesToProcess>ExpectedByteLength) {
printf("\nWarning, %u byte message longer than expected %u bytes",BytesToProcess,ExpectedByteLength);
		BytesToProcess = ExpectedByteLength;
	}
/*
*	If message long enough but the checksum is wrong, drop the message
*/
	if (gefCalculateCRChecksum (BytesToProcess, SerialBuffer)) {
printf("\nError, Bad Checksum %u bytes",BytesToProcess);
		BytesToProcess = 0;
		return(0);
	}
	ResponseLength = gefProcessMODBUSData(BytesToProcess, SerialBuffer);
	if (ResponseLength>0) {
		cb_putblock(SerialBuffer, ResponseLength,pSerialPort);			
	}
	if ((Function==15)||(Function==16)) {
		DataBytesTransferred = BytesToProcess - 9;
	}
	else {                   
		DataBytesTransferred = 0;
		if (ResponseLength>9) {
			DataBytesTransferred = ResponseLength - 89;
		}
	}
	BytesToProcess = 0;		   
	return(DataBytesTransferred);
}	
enum {RTU_EXCEPTION_ILLEGAL_FUNCTION=1,
	RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS=2,
	RTU_EXCEPTION_ILLEGAL_DATA_VALUE=3,
	RTU_EXCEPTION_ILLEGAL_RESPONSE_LENGTH=4};
int gefProcessMODBUSData(int ByteLength, BYTE *pBufferStart)
/*
*	Procedure to process MODBUS messages based on function code and
*	reference address. The procedure is called with a RTU formatted request
*	in pBufferStart. It returns a RTU formatted response in pBufferStart
*	with the response byte length as the function value. The procedure is
*	designed to be used with MODBUS RTU serial or MODBUS/TCP or UDP messages
*/
{	
	int Station,Function,DataAddress,DataLength,ResponseLength,ByteCount;
	int ExceptionCode,CheckSum,MaxAddress,Loop,BitShiftLeft,BitMask;
	int DataBytesTransferred,ResponseByteLength,ValueInTable,ValueInMessage;
	int UseExpandedDataLength;
	short Value16Bit;
	BYTE *pBuffer;
	BYTE *pDataBytes;
	GEF_MODBUS_RTU_REQUEST *pRequest;
	
	pRequest = (GEF_MODBUS_RTU_REQUEST *)pBufferStart;
	pBuffer = pBufferStart + 2;
	ExceptionCode = 0;
		
	Station = pRequest->UnitIdentifier;
	Function = pRequest->MODBUSFunctionCode;	
	DataAddress = (pRequest->ReferenceNumberHi<<8) | pRequest->ReferenceNumberLo;
	DataLength = (pRequest->DataLengthHi<<8) | pRequest->DataLengthLo;
//	OutputBuffer[0] = pRequest->UnitIdentifier;
//	OutputBuffer[1] = pRequest->MODBUSFunctionCode;	
	ResponseLength = 3;       
	switch (Function) {
	case 1:
	case 2:
/*
*	Read %Q output or %I input bit data, shifting bytes if not on byte boundary
*/
		if (Function==1) {
			pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryQ];
			MaxAddress = MemoryTableLength[MODBUSMemoryQ]<<3;
		}
		else {
			pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryI];
			MaxAddress = MemoryTableLength[MODBUSMemoryI]<<3;
		}
		if ((DataLength<0)||((DataAddress+DataLength)>MaxAddress)) {
printf("\nError, Address %u or Length %u out of range %u",DataAddress,DataLength,MaxAddress);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {
			DataBytesTransferred  = (DataLength + 7)>>3; 
			if (DataBytesTransferred<=250) {
				*pBuffer++ = (BYTE)DataBytesTransferred;
				UseExpandedDataLength = 0;
			}
			else {
				*pBuffer++ = 0xFF;
				*((WORD *)pBuffer)++ = (WORD)DataBytesTransferred;
				UseExpandedDataLength = 2;
//				*pBuffer)++ = (BYTE)(DataBytesTransferred>>8);
//				*pBuffer++ = (BYTE)DataBytesTransferred;
			}
			pDataBytes += DataAddress>>3;
			BitShiftLeft = DataAddress&7;
			for (Loop=0; Loop<DataBytesTransferred; Loop++) {
				if (BitShiftLeft) {
					Value16Bit = *(short *)pDataBytes;
					*pBuffer++ = (BYTE) (Value16Bit<<BitShiftLeft);
					pDataBytes++;
				}
				else {
					*pBuffer++ = *pDataBytes++;
				}
			}			
//			memcpy(pBuffer,pDataBytes,DataBytesTransferred);
//			pBuffer += DataBytesTransferred;
			ResponseByteLength = DataBytesTransferred + UseExpandedDataLength + 3;
		}
		break;
	case 3:
	case 4:   
/*
*	Read %R or %AI word data, swapping bytes. Gateway will use 3 for %AI and 4 for %R
*/
		if (Function==3) {
			pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryR];
			MaxAddress = MemoryTableLength[MODBUSMemoryR]>>1;
		}
		else {
			pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryAI];
			MaxAddress = MemoryTableLength[MODBUSMemoryAI]>>1;
		}
		if ((DataLength<0)||((DataAddress+DataLength)>MaxAddress)) {
printf("\nError, Address %u or Length %u out of range %u",DataAddress,DataLength,MaxAddress);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {
			DataBytesTransferred  = DataLength<<1;
			if (DataBytesTransferred<=125) {
				*pBuffer++ = (BYTE)DataBytesTransferred;
				UseExpandedDataLength = 0;
			}
			else {
				*pBuffer++ = 0xFF;
				*((WORD *)pBuffer)++ = (WORD)DataBytesTransferred;
				UseExpandedDataLength = 2;
//				*pBuffer)++ = (BYTE)(DataBytesTransferred>>8);
//				*pBuffer++ = (BYTE)DataBytesTransferred;
			}
			pDataBytes += DataAddress<<1;
			for (Loop=0; Loop<DataLength; Loop++) {
				*pBuffer++ = *(pDataBytes+1);
				*pBuffer++ = *pDataBytes;
				pDataBytes += 2;
			}
			ResponseByteLength = DataBytesTransferred + UseExpandedDataLength + 3;
//			_swab(pDataBytes, pBuffer,DataBytesTransferred);
//			pBuffer += DataBytesTransferred;
		}
		break;
	case 5:
/*
*	Write single %Q bit based value in 5th byte, same as upper byte in DataLength
*/
		pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryQ];
		MaxAddress = MemoryTableLength[MODBUSMemoryQ]<<3;
		if (DataAddress>MaxAddress) {
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {                                      
			BitMask = 1<<(DataAddress&7);
			ValueInTable = (*pDataBytes)&BitMask;
			ValueInMessage = (DataLength ? BitMask : 0);
			if (ValueInTable!=ValueInMessage) {
				*pDataBytes ^= BitMask;
			}
			ResponseByteLength = 6;
		}
		break;			
	case 6:   
/*
*	Write single %R word to value in DataLength. Will switch the %R to %AQ for gateway
*/
		pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryR];
		MaxAddress = MemoryTableLength[MODBUSMemoryR]>>1;
		if (DataAddress>MaxAddress) {
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {
			*(short *)pDataBytes = (short)DataLength;			
			ResponseByteLength = 6;
		}
		break;
	case 7:
/*
*	Read Exception status returns 8 bits starting at %Q1
*/
		pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryQ];
		*pBuffer = *pDataBytes;
		ResponseByteLength = 3;
		break;
	case 8:
/*
*	Loopback/mainenance test echos back same data
*/
		ResponseByteLength = 6;
		break;
	case 15:
/*
*	Write to multiple %Q, shifting bytes if not on byte boundary
*/
		pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryQ];
		MaxAddress = MemoryTableLength[MODBUSMemoryQ]<<3;
		DataBytesTransferred  = (DataLength + 7)>>3;                    
		pBuffer += 4;
		ByteCount = *pBuffer++;
		if (ByteCount==0xFF) {
			ByteCount = *((WORD *)pBuffer)++;
		}
		if ((DataLength<0)||((DataAddress+DataLength)>MaxAddress)) {
printf("\nError, Address %u or Length %u out of range %u",DataAddress,DataLength,MaxAddress);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (ByteCount!=DataBytesTransferred) {
printf("\nError, %u ByteCount not equal to %u Data bytes",ByteCount,DataBytesTransferred);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {
			pDataBytes += DataAddress>>3;
			BitShiftLeft = DataAddress&7;
/*
*	DEBUG WARNING, THIS ONLY HANDLES BYTE LENGTH ON BYTE BOUNDARY FOR NOW
*/
			for (Loop=0; Loop<DataBytesTransferred; Loop++) {
				if (BitShiftLeft) {
					Value16Bit = *(short *)pBuffer;
					*pDataBytes++ = (BYTE) (Value16Bit<<BitShiftLeft);
					pBuffer++;
				}
				else {
					*pDataBytes++ = *pBuffer++;
				}
			}			
			ResponseByteLength = 6;
//			memcpy(pBuffer,pDataBytes,DataBytesTransferred);
//			pBuffer += DataBytesTransferred;
		}
		break;
	case 16:
/*
*	Write to multiple %R swapping bytes. Gateway will write to %AQ
*/
		pDataBytes = (BYTE *)pMemoryTableStart[MODBUSMemoryR];
		MaxAddress = MemoryTableLength[MODBUSMemoryR]>>1;
		DataBytesTransferred  = DataLength<<1;
		pBuffer += 4;
		ByteCount = *pBuffer++;
		if (ByteCount==0xFF) {
			ByteCount = *((WORD *)pBuffer)++;
		}
		if ((DataLength<0)||((DataAddress+DataLength)>MaxAddress)) {
printf("\nError, Address %u or Length %u out of range %u",DataAddress,DataLength,MaxAddress);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (ByteCount!=DataBytesTransferred) {
printf("\nError, %u ByteCount not equal to %u Data bytes",ByteCount,DataBytesTransferred);
			ExceptionCode = RTU_EXCEPTION_ILLEGAL_DATA_ADDRESS;
		}                                                        
		if (!ExceptionCode) {
			pDataBytes += DataAddress<<1;
			pDataBytes += DataAddress<<1;
			for (Loop=0; Loop<DataLength; Loop++) {
				*pDataBytes++ = *(pBuffer+1);
				*pDataBytes++ = *pBuffer;
				pBuffer += 2;
			}
			ResponseByteLength = 6;
//			memcpy(pDataBytes,pBufferDataBytesTransferred);
//			pBuffer += DataBytesTransferred;
		}
		break;

	default:
		ExceptionCode = RTU_EXCEPTION_ILLEGAL_FUNCTION;
		break;
	}                                                          
/*
*	Check if exception code to generate error response
*/
	if (ExceptionCode) {
		*(pBufferStart+2) = (BYTE)ExceptionCode;
		ResponseByteLength = 3;
	}
	else {
printf("\nStation %u Function %u Address %u DataLength %u",Station,Function,DataAddress,DataLength);
	}
/*
*	Add check sum to end of message and return the total response byte length
*/
	CheckSum = gefCalculateCRChecksum (ResponseByteLength, pBufferStart);
	*((WORD *)(pBufferStart+ResponseByteLength)) = CheckSum;
	ResponseByteLength += 2;
	return(ResponseByteLength);
}		
int gefProcessMasterRTU(CCB *pSerialPort, int SlaveID, int Function, int DataAddress, 
						int DataLength, BYTE *pDataBytesStart, int CharacterTimeoutMS)
/*
*	RTU Master routine creates RTU request for Slave ID and sends it if Function is
*	not 0. The expected byte length of the reply is returned as the function value.
*	The calling process must continue called the same routine periodically
*	(at least every tick) with Finction=0 and DataLength = exspected byte length.
*	The routine checks receive buffer to build reply string and returns 0
*	if less than Datalength byte. Once the correct data length is reached or there
*	is a timeout, the reply is processed and the byte length is returned indicating
*	the request is complete. A negative return code is an error.
*/
{
	int BytesIn,BytesOut,DataBytesTransferred;
	int RequestByteLength,ResponseByteLength,DeltaMS;
	int BitShiftLeft,CheckSum,Loop;
	short Value16Bit;
	static BYTE SerialBuffer[300];
	static int BytesToProcess=0;         
	clock_t CurrentTime;
	static clock_t LastTime=0;
	BYTE *pBuffer;
	BYTE *pDataBytes;
	GEF_MODBUS_RTU_REQUEST *pRequest;

	pRequest = (GEF_MODBUS_RTU_REQUEST *)SerialBuffer;
	pDataBytes = pDataBytesStart;
	if (Function>0) {
		RequestByteLength = 6;
		ResponseByteLength = 8;
		pRequest->UnitIdentifier = (BYTE)SlaveID;
		pRequest->MODBUSFunctionCode = (BYTE)Function;
		pRequest->ReferenceNumberHi = (BYTE)(DataAddress>>8);
		pRequest->ReferenceNumberLo = (BYTE)DataAddress;
		pRequest->DataLengthHi = (BYTE)(DataLength>>8);
		pRequest->DataLengthLo = (BYTE)DataLength;
		switch (Function) {
		case 1:
		case 2:
			DataBytesTransferred  = (DataLength + 7)>>3; 
			break;
		case 3:
		case 4:
			DataBytesTransferred  = (DataLength + 7)>>3; 
			ResponseByteLength = (DataLength<<1) + 5;
			if (ResponseByteLength>260) {
				ResponseByteLength += 2;
			}
			break;
		case 8:
			break;
		case 67:
			ResponseByteLength = DataLength + 5;
			break;
		case 5:
			pRequest->DataLengthHi = 0;
			pRequest->DataLengthLo = 0;
			BitShiftLeft = 1<<(DataAddress&7);
			if (*pDataBytes&BitShiftLeft) {
				pRequest->DataLengthHi = (BYTE)0xFF;
			}
			break;
		case 6:
			Value16Bit = *(short *)pDataBytes;
			pRequest->DataLengthHi = (BYTE)(Value16Bit>>8);
			pRequest->DataLengthLo = (BYTE)Value16Bit;
			break;
		case 7:
		case 17:
			RequestByteLength = 2;
			ResponseByteLength = ((Function==7) ? 4 : 10);
			break;
		case 15:
			DataBytesTransferred  = (DataLength + 7)>>3; 
			pBuffer = &SerialBuffer[6];
			if (DataBytesTransferred<=250) {
				*pBuffer++ = (BYTE)DataBytesTransferred;
				RequestByteLength = 7;
			}
			else {
				*pBuffer++ = 0xFF;
				*((WORD *)pBuffer)++ = (WORD)DataBytesTransferred;
				RequestByteLength = 9;
			}
			BitShiftLeft = DataAddress&7;
			for (Loop=0; Loop<DataBytesTransferred; Loop++) {
				if (BitShiftLeft) {
					Value16Bit = *(short *)pDataBytes;
					*pBuffer++ = (BYTE) (Value16Bit<<BitShiftLeft);
					pDataBytes++;
				}
				else {
					*pBuffer++ = *pDataBytes++;
				}
			}			
			RequestByteLength += DataBytesTransferred;
			break;
		case 16:
			DataBytesTransferred  = DataLength<<1;
			pBuffer = &SerialBuffer[6];
			if (DataBytesTransferred<=250) {
				*pBuffer++ = (BYTE)DataBytesTransferred;
				RequestByteLength = 7;
			}
			else {
				*pBuffer++ = 0xFF;
				*((WORD *)pBuffer)++ = (WORD)DataBytesTransferred;
				RequestByteLength = 9;
			}
			for (Loop=0; Loop<DataLength; Loop++) {
				*pBuffer++ = *(pDataBytes+1);
				*pBuffer++ = *pDataBytes;
				pDataBytes += 2;
			}
			RequestByteLength += DataBytesTransferred;
			break;
		default:
			return(0);
		}
/*
*	Add check sum to end of message and return the total response byte length
*/
		CheckSum = gefCalculateCRChecksum (RequestByteLength, SerialBuffer);
		*((WORD *)(&SerialBuffer[RequestByteLength])) = CheckSum;
		cb_putblock(SerialBuffer,RequestByteLength+2,pSerialPort);
		return(ResponseByteLength);
	}
/*
*	Return if transmitting data or nothing in receive queue
*	Reset input buffer being processed if character timeout
*/                 
	BytesIn = cb_rxcib(pSerialPort);
	BytesOut = cb_txcib(pSerialPort);
	if ((BytesOut>0)||(BytesIn<=0)) {
		return(0);
	}	
/*
*	Check time passed sine last characters. If too long, drop message
*	THIS IS TO DROP PARTIAL MESSAGES OR ONES WITH GAPS. NOT DONE
*	NEED TO USE THE HIGHEST RESOLUTION CLOCK AS GAP IS 2 MS at 19200
*/
	CurrentTime = clock();
	DeltaMS = (int)((1000*(CurrentTime - LastTime))/CLOCKS_PER_SEC);
	if (DeltaMS>CharacterTimeoutMS) {
		BytesToProcess = 0;
	}
	LastTime = CurrentTime;
/*
*	Add new input data to buffer, adjusting start to fit(error condition)
*/ 
	if ((BytesToProcess+BytesIn)>sizeof(SerialBuffer)) {
		BytesToProcess = sizeof(SerialBuffer) - BytesIn;
	}
	cb_getblock(&SerialBuffer[BytesToProcess],BytesIn,pSerialPort);
//printf("Add %u to Buff[%u]",BytesIn,BytesToProcess);
	BytesToProcess  += BytesIn;
	if (BytesToProcess<DataLength) {
		return(0);
	}
	return(0);
}