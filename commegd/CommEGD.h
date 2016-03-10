/*
*	CommEGD.h is a temporary header file for GE Fanuc PLC Tech Support
*	EGD routines. It may be merged with the complete GEFComm.h file
*	after that header file is cleaned up to remove extra structures
*	and fields that will never be used. This is a subset for EGD
*
*	Doug MacLeod, GE Fanuc PLC Tech Support. Jul 2000
*/
//#define INCLUDE_GENIGATE	// Uncomment to add GENIGate support for PCIM and RTU
#ifndef _INC_GEF_COMMEGD
#ifdef __cplusplus
extern "C" {
#endif
#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4273)   /* kill warning using import and export together*/
#define DllImport __declspec(dllimport)
#define DllExport __declspec(dllexport)
#define sprintf  wsprintf		/* to use Windows rather than C run time library */
#include <conio.h>
#pragma intrinsic (_inp,_outp,memcmp,memcpy,memset)
//	Constants and functions to emulate CommBLOK library under Windows
#define CCB				HANDLE
#define PTP_MODE		0
#define CTS_RTS_CTRL	2
int cb_hwsetup(int COMPort, WORD Port, int IRQ, int SizeTX, int SizeRX);
CCB *cb_open(int COMPort, long DataRate, char Parity, int StopBits, int DataBits,
	int DTRHigh, int RTSHigh, int Mode);
char *cb_getblock(char *pString, int ByteCount, CCB *pSerial);
char *cb_putblock(char *pString, int ByteCount, CCB *pSerial);
int cb_rxcib(CCB *pSerial);
int cb_txcib(CCB *pSerial);
int cb_clrbreak(CCB *pSerial);
int cb_setbreak(CCB *pSerial);
#else
#define DllImport
#define DllExport
#define WINAPI _far _pascal
#define HWND int
#define BOOL int
#define HINSTANCE int
#define HANDLE int
#define OVERLAPPED int
#define WSADATA int
#define TRUE 1
#define FALSE 0
#define PVOID void *
#include <memory.h>
#include <string.h>
#include "commblok.h"	/* include file for the 16-bit Drumlin CommBLOK library */
#endif
#pragma pack(1)		// change default packing from 4 or 8 bytes to 1 for MODBUS RTU header
#define GEF_EGD_UDP_DATA_PORT  0x4746	/* Letters GF used as port for EGD Data message */
#define GEF_MAX_GENICARD_NUMBER 4			/* Upper limit to allocate space for GENI cards */
//	Structure for GE Fanuc Ethernet Global Data or EGD (Data Port only)
typedef  struct {
	unsigned short	PDUTypeVersion;		/* Type=13 (0Dh) in low byte, version=1 in hi */
	unsigned short	RequestID;			/* incremented every time data produced */
	unsigned long	ProducerID;			/* The TCP/IP address of device sending EGD */
	unsigned long	ExchangeID;			/* A unique Producer number identifying the data */
	unsigned long	TimeStampSec;		/* Timestamp seconds since 1-Jan-1970 */
	unsigned long	TimeStampNanoSec;	/* and number of nanoseconds in current second */
	unsigned long	Status;				/* In low word, upper word reserved */
	unsigned long	ConfigSignature;	/* In low word, upper word reserved */
	unsigned long	Reserved;
	unsigned char	ProductionData[1400];
} GEF_EGD_DATA;
typedef struct {
	DWORD	ProducerTCPIP;	// return TCP/IP address for DeviceType/Number
	DWORD	ExchangeID;		// return same value sent as parameter
	DWORD	ExchangeCount;	// sent or received, can 0 if fWrite is TRUE
	DWORD	ErrorCount;		// Timeouts for consumer, other producer errors
	DWORD	TimeStampSec;	// EGD time stamp for last message received
	DWORD	TimeStampNanoSec;
	long	TimeTillTransferEGD;	// MilliSec remaining, minus if overdue;
	WORD	RequestID;		// Producer increments when EGD message sent
	short	EnableExchange;	// set to 1 to enable, 0 to disable
} GEF_EGD_EXCHANGE_STATUS;
typedef struct {
	short	SNPMemoryType;	// SNP Memory Type, %AI=10, %I=16, etc
	short	StartAddress;	// Addresses start at 1
	short	DataLength;		// In words or bits if SNPMemoryType greater than %AQ
} GEF_PLC_MEMORY_LIST;
typedef struct {
	DWORD	ProducerTCPIP;	// If equal to computer TCP/IP, it is producer
	DWORD	ExchangeID;		// Unique number for each producer from 1 to N
	short	DeviceType;		// 0=CPU, 1=PLC, 2=GIO, 3=EIO, etc
	short	DeviceNumber;	// 1 to 9999
	DWORD	ConsumerTCPIP;	// Set to 0 if this computer is consuming
	DWORD	ProducerPeriod;	// 10 to 3600000 (1 hour) milliseconds 
	DWORD	ConsumerTimeout; // 10 to 3600000 milliseconds for timeout, 0=none
	short	DataByteLength;		// configured transfer length, EGD limit 1400
	short	MemoryListCount; // same as AddressSegment count
	long	PLCStatusTypeAddress;	// SNP Type in upper, address in lower
	short	DataBytesReceived;	// Set when first EGD message received
} GEF_EGD_EXCHANGE_CONFIG;
typedef struct {
	short	SNPMemoryType;	// SNP Memory Type, %AI=10, %I=16, etc
	short	StartAddress;	// Addresses start at 1
	short	DataByteLength;	// Byte length of data in this segment
	short	ByteOffsetInMessage;  // Optional, if left at 0, calculated as messages transferred
	BYTE	*pDataTable;	// Pointer to local computer memory for data
	BYTE	*pOverrideTable;	// Pointer to local computer memory for discrete overrides
} GEF_EGD_ADDRESS_SEGMENT;
#define GEF_SHIFT_DEVICE_TYPE 10000000
typedef struct {
//	First part lined up to match GEF_EGD_EXCHANGE_STATUS data
	DWORD	ProducerTCPIP;
	DWORD	ExchangeID;
	DWORD	ExchangeCount;
	DWORD	TimeStampSec;
	DWORD	TimeStampNanoSec;
	long	TimeTillTransferEGD;	// MilliSec remaining, minus if overdue;
	WORD	RequestID;		// Producer increments whenever EGD message sent
	short	EnableExchange;	// set to 1 to enable, 0 to disable
//	Next part lined up to match GEF_EGD_EXCHANGE_CONFIG data
	long	ProducerDeviceTypeNumber;	// Type * GEF_SHIFT_DEVICE_TYPE + DeviceNumber
	long	ConsumerDeviceTypeNumber;	// used to link producer and consumer
	DWORD	ProducerPeriod;		// Produce at 10 to 3600000 MS, 0 for Consume
	DWORD	ConsumerTimeout;	// Consume 10 to 3600000 MS, 0 for no timeout
	DWORD	ConsumerTCPIP;
	short	DataByteLength;		// configured data bytes transferred, EGD limited to 1400
	short	AddressSegmentCount; // 
	GEF_EGD_ADDRESS_SEGMENT *pAddressSegment; // expanded copy of PLC_MEMORY_LIST
	long	PLCStatusTypeAddress;	// SNP Type in upper, address in lower word
	short	*pPLCStatusExchange;	// if not-NULL, points to Exchange status location
	short	DataBytesReceived;	// Set when first EGD message received
} GEF_EGD_EXCHANGE_LIST;
typedef  struct {
	int		SNPMemoryType;			// normal SNP memory type
	int		SNPMemoryTypePacked;	// bit packed for discrete
	int		SNPMemoryTypeOverride;	// override tables for most discrete
	char	MemoryTypeLetters[4];	// NULL terminated
	int		HighestPLCAddress;		// from _MAIN.DEC file
	int		FormatByteLength;		// 1 per 8 bits or single word
	BYTE	*pStartOfData;			// will be (short *) for words
	BYTE	*pStartOfOverrides;
	BYTE	*pStartOfFormat;
} GEF_PLC_REF_TABLE_TYPES;
extern GEF_PLC_REF_TABLE_TYPES gefRefTableTypes[14];
typedef struct {
//	short   TypeCardDevice;		/* Type hi nibble, Card 2nd Device lo byte */
/*	Type 0=GENI,1=PCIF1,2=PCIF2,  Card=1 to 15, For PCIF Device=RackSlot */
	short	TypeCard;          	/* Type in high byte, Card 1 to 16 in low */
	short	RackDeviceSlot;		/* Rack or Device in high byte, slot in low */
	long	ModuleSortOrder;		/* Type, Card, Rack, Device, Slot */
	long	SectionStart;
	short	SectionLength;
	WORD    ModuleID;			/* Genius model # or 90-30 pseudo-ID */
	short	LengthI;
	short	AddressI;
	short	LengthAI;
	short	AddressAI;
	short	LengthRI;			/* RI and RQ are for some Horner modules */
	short	AddressRI;
	short	LengthQ;
	short	AddressQ;
	short	LengthAQ;
	short	AddressAQ;
	short	LengthRQ;
	short	AddressRQ;
	BYTE	ConfigData[12];		/* 11 bytes used for smart modules */
	short	InitByteCount;		/* for HSC, APM/DSM, smart analog */
	BYTE far *pInitData;
	char    PartNumber[16]; 
} GEF_MODULE_CONFIG;			/* This will be expanded */
typedef struct {
	int		ByteLengthI;
	int		ByteLengthAI;
	int		ByteLengthQ;
	int		ByteLengthAQ;
	BYTE far *pDataI;		// location of data in computer reference tables;
	BYTE far *pDataAI;
	BYTE far *pDataQ;
	BYTE far *pDataAQ;
} GEF_PLC_TRANSFER_IO;
typedef struct {
	int		PortAddress;
	BYTE far *pGENIMemory;	// same as GEMI_MEMORY in the 16-bit GENILib
	int		Interrupt;		// normally 0 fro no interrupt
	int		HostSBA;		// read from DIP switch after GENI turned on
	WORD	EnableOutputs[2];	// From SBA 32 Q(32) bits, update GEN when changed
	GEF_PLC_TRANSFER_IO DeviceIO[33];  /* SBA 0 to 31, 32 is Global Broadcast/Directed Input */
} GEF_GENI_CARD_CONFIG;
typedef struct {
	WORD	TransactionIdentifier;	/* copied by server, usually 0 */
	WORD	ProtocolIdentifier;		/* 0 for MODBUS TCP */           
	WORD	LengthField;			/* Standard MODUB, max 256 bytes */
} GEF_MODBUS_TCP_HEADER;
typedef struct {
	BYTE	UnitIdentifier;
	BYTE	MODBUSFunctionCode;
	BYTE	ReferenceNumberHi;
	BYTE	ReferenceNumberLo;
	BYTE	DataLengthHi;
	BYTE	DataLengthLo;
	WORD	CheckSum;
} GEF_MODBUS_RTU_REQUEST;
typedef struct {
	BYTE	UnitIdentifier;
	BYTE	MODBUSFunctionCode;
	BYTE	ExceptionCode;
	WORD	CheckSum;
} GEF_MODBUS_RTU_EXCEPTION;
#pragma pack()
int WINAPI gefEGDLoadConfig(DWORD DefaultHostTCPIP, int MaxExchangeCount);
int WINAPI gefEGDConfig( int fWrite, int IndexBased0, GEF_EGD_EXCHANGE_CONFIG *pExchangeConfig,
						int TextBytes, char *pTextLine);
int WINAPI gefEGDStatus( int fWrite, int IndexBased0, GEF_EGD_EXCHANGE_STATUS *pExchangeStatus,
					   int TextBytes, char *pTextLine);
int WINAPI gefEGDMemoryList( int fWrite, GEF_EGD_EXCHANGE_CONFIG *pExchangeConfig, 
	int MemoryListCount, GEF_PLC_MEMORY_LIST *pMemoryListStart, int TextBytes, char *pTextLine);
//	This are the EGD versions of gefRead/WritePLCMemory with PLCNumber = 0
int WINAPI gefReadComputerMemory(int MemoryType,	int PLCAddress,	int DataLength, short far *pBuffer);
int WINAPI gefWriteComputerMemory(int MemoryType, int PLCAddress, int DataLength, short far *pBuffer);
int gefLoadGENIConfigFile(int GENICardNumber, char *pConfigFileName);
int gefTransferGENIDeviceIO(int GENICardNumber, int fWriteData);
int gefProcessMODBUSData(int ByteLength, BYTE *pBufferStart);
CCB *gefSetupSerialPort(int COMPort, long DataRate, int ParityStopBits);
int gefProcessSlaveRTU(CCB *pSerialPort, int SlaveID, int CharacterTimeoutMS);
//int gefLoadConfigFile(char *pConfigFileName, int MaxModuleCount,
//		GEF_MODULE_CONFIG *pModuleConfigStart, GEF_GENI_CARD_CONFIG *pGENICardConfig);
/* PLCMemoryType symbols from GEFComm.h */
int gefCheckGEFCommDriver(void);
DllImport int WINAPI gefActivateGENIDevices ( int StartingDeviceNumber, 
					int NumberOfDevices, int CloseResetAction);
#define PLCMemoryTypeAI     10
#define PLCMemoryTypeAQ     12
#define PLCMemoryTypeAIover 62      /* Analog overrides for PCIF only */
#define PLCMemoryTypeAQover 64
#define PLCMemoryTypeG      86
#define PLCMemoryTypeGover  166
#define PLCMemoryTypeGpack  56
#define PLCMemoryTypeI      70
#define PLCMemoryTypeIover  150
#define PLCMemoryTypeIpack  16
#define PLCMemoryTypeL       0
#define PLCMemoryTypeM      76
#define PLCMemoryTypeMover  156
#define PLCMemoryTypeMpack  22
#define PLCMemoryTypeP       4
#define PLCMemoryTypeQ      72
#define PLCMemoryTypeQover  152
#define PLCMemoryTypeQpack  18
#define PLCMemoryTypeR       8
#define PLCMemoryTypeS      84
#define PLCMemoryTypeSover  164
#define PLCMemoryTypeSpack  30
#define PLCMemoryTypeSA     78
#define PLCMemoryTypeSAover 158
#define PLCMemoryTypeSApack 24
#define PLCMemoryTypeSB     80
#define PLCMemoryTypeSBover 160
#define PLCMemoryTypeSBpack 26
#define PLCMemoryTypeSC     82
#define PLCMemoryTypeSCover 162
#define PLCMemoryTypeSCpack 28
#define PLCMemoryTypeT      74
#define PLCMemoryTypeTover  154
#define PLCMemoryTypeTpack  20
#define PLCMemoryTypeMret	200     /* Retentive M/Q tables code are NOT actual SNP types */
#define PLCMemoryTypeQret	202     /* and use MR or QR for type letters. Use symbols only */
#ifdef __cplusplus
}
#endif
#define _INC_GEF_COMMEGD
#endif
