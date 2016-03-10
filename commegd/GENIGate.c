/*
*	The GENIGate program provides a gateway between GE Fanuc Genius (and future Series 90-30 PCIF)
*	I/O and Ethernet TCP/IP networks using GE Fanuc Ethernet Global Data (EGD) Future MODBUS UDP/RTU
*	This is 32-bit code supporting PCIM (and PCIF cards) in a computer running Windows NT/2000 or
*	Windos 95/98 using the GENICard.sys/VxD drivers. A 16-bit version will be available on a
*	25 MHz microcontroller with embedded UDP Ethernet and Genius PC-104 cards (future PCIF 104 card)
*
*	THIS CODE IS CURRENTLY BEING TESTED AND IS NOT DONE. The gef_agd.c and gef_agh.c need to be updated
*	for the microcontroller
*/
#include <windows.h>
//#include "gefcomm.h"
#include "CommEGD.h"	// subset of GEFComm.h for EGD
#include "GENILib.h"
#include <stdio.h>
#include <time.h>		// for time function to get system time for time stamp
#include <process.h>	// For _beginthread call to set up seprate EGD processing thread
#pragma pack(1)
#define GEF_MAX_INI_LINE_LENGTH 4096
#define GEF_MAX_TCPIP_LIST 10000	// Limit to 10000 Ethernet devices on network
#define GEF_SHIFT_DEVICE_TYPE 10000000
typedef struct {
	DWORD	TCPIP;
	long	DeviceTypeNumber;	// Type * GEF_SHIFT_DEVICE_TYPE + DeviceNumber
} GEF_TCPIP_LIST;
typedef struct {
	int		Command;	// 0=Run, 1=RequestSuspend, 2=Suspended, 3=RequestExit, 4=Exited
	GEF_EGD_EXCHANGE_LIST *pEGDExchangeListStart;
	int		ExchangeListCount;
	int		MaxExchangeListCount;	// upper limit set when pEGDExchangeListStart setup
	SOCKET	HostSocket;
	SOCKET	TargetSocket;
	int		LastWinsockError;	// Save the WSAGetLastEror for Winsock
	DWORD	LoadTCPIP;		// Host computer TCP/IP to identify which exchanges are produced
} GEF_EGD_THREAD_DATA;
//	Local private data storage and internal routines. The RefTableTypes is from
//	gefTable.c code to provide LM90 style reference tables. Computer memory is allocated
//	statically and set to 0 based on large PLC.. In future it will load from ref table files 
typedef struct {
	BYTE    I[1536];
	BYTE    IOver[1536];
	BYTE    Q[1536];
	BYTE    QOver[1536];
//	BYTE    QRetM[1536];
	BYTE    M[1536];
	BYTE    MOver[1536];
//	BYTE    MRetM[1536];
	BYTE    G[960];		// GABCDE follow G[160]
	BYTE    GOver[160];
//	BYTE    T[32];
//	BYTE    TOver[32];	// DEBUG is there an override for T's
//	BYTE    S[32];
//	BYTE    SA[32];
//	BYTE    SB[32];
//	BYTE    SC[32];
//	BYTE    SOver[32];	// The system override tables are not used yet
//	BYTE    SAOver[32];
//	BYTE    SBOver[32];
//	BYTE    SCOver[32];
	short   AI[8192];
	short   AQ[8192];
	short   R[32768];
//	short   P[8192];
//	short   L[8192];
//	BYTE	FormatI[1536];
//	BYTE	FormatQ[1536];
//	BYTE	FormatM[1536];
//	BYTE	FormatG[960];	// GABCDE follow G
//	BYTE	FormatT[32];
//	BYTE	FormatAI[8192];
//	BYTE	FormatAQ[8192];
//	BYTE	FormatR[32768];
//	BYTE	FormatP[8192];
//	BYTE	FormatL[8192];
//	BYTE	FormatS[32];
//	BYTE	FormatSA[32];
//	BYTE	FormatSB[32];
//	BYTE	FormatSC[32];
} GEF_PLC_REF_TABLE_BUFFER_X;
static GEF_PLC_REF_TABLE_BUFFER_X gMem;
static GEF_EGD_THREAD_DATA gefEGDThreadData;
static GEF_EGD_DATA MessageEGD;
static const char *pSectionTypes[]={"CPU","PLC","GIO","EIO","CIO","DIO","PIO","AIO"};
int MaxModuleCount = 100;
static GEF_MODULE_CONFIG *pModuleConfigStart=NULL;
static GEF_GENI_CARD_CONFIG gefGENICardConfig[GEF_MAX_GENICARD_NUMBER];
void gefCommProcessEGDThread(GEF_EGD_THREAD_DATA *pThreadData);
int gefScanAddressSegments(int fProducerLine, DWORD LoadTCPIP, char *pTextLine, GEF_EGD_EXCHANGE_LIST *pExchangeList);
long WINAPI gefParseDeviceTypeNumber(long DeviceTypeNumber, char *pDeviceTypeNumber);
#ifndef GEF_GATEWAY		// In GENIGate test only
GEF_PLC_REF_TABLE_TYPES gefRefTableTypes[] = {
	PLCMemoryTypeI,PLCMemoryTypeIpack,PLCMemoryTypeIover, "I", 12288, 1536, gMem.I, gMem.IOver, NULL, //gMem.FormatI,
	PLCMemoryTypeQ,PLCMemoryTypeQpack,PLCMemoryTypeQover, "Q", 12288, 1536, gMem.Q, gMem.QOver, NULL, //gMem.FormatQ,
	PLCMemoryTypeM,PLCMemoryTypeMpack,PLCMemoryTypeMover, "M", 12288, 1536, gMem.M, gMem.MOver, NULL, //gMem.FormatM,
	PLCMemoryTypeG,PLCMemoryTypeGpack,PLCMemoryTypeGover, "G", 7680, 960, gMem.G, gMem.GOver, NULL, //gMem.FormatG,
//	PLCMemoryTypeT,PLCMemoryTypeTpack,PLCMemoryTypeTover, "T", 256, 32, gMem.T, gMem.TOver, gMem.FormatT,
	PLCMemoryTypeAI,PLCMemoryTypeAI,0, "AI", 8192, 8192, (BYTE *)gMem.AI, NULL, NULL, //gMem.FormatAI,
	PLCMemoryTypeAQ,PLCMemoryTypeAQ,0, "AQ", 8192, 8192, (BYTE *)gMem.AQ, NULL, NULL, //gMem.FormatAQ,
	PLCMemoryTypeR,PLCMemoryTypeR,0, "R", 32768, 32768, (BYTE *)gMem.R, NULL, NULL, //gMem.FormatR,
//	PLCMemoryTypeP,PLCMemoryTypeP,0, "P", 0, 0,        (BYTE *)gMem.P, NULL, gMem.FormatP,
//	PLCMemoryTypeL,PLCMemoryTypeL,0, "L", 0, 0, (BYTE *)gMem.L, NULL, gMem.FormatL,
//	PLCMemoryTypeS,PLCMemoryTypeSpack,PLCMemoryTypeSover, "S", 256, 32, gMem.S, gMem.SOver, gMem.FormatS,
//	PLCMemoryTypeSA,PLCMemoryTypeSApack,PLCMemoryTypeSAover, "SA", 256, 32, gMem.SA, gMem.SAOver, gMem.FormatSA,
//	PLCMemoryTypeSB,PLCMemoryTypeSBpack,PLCMemoryTypeSBover, "SB", 256, 32, gMem.SB, gMem.SBOver, gMem.FormatSB,
//	PLCMemoryTypeSC,PLCMemoryTypeSCpack,PLCMemoryTypeSCover, "SC", 256, 32, gMem.SC, gMem.SCOver, gMem.FormatSC,
};
int gefCheckGEFCommDriver(void);
int main (int argc, char *argv[])
{
	int Cards;

	gefCheckGEFCommDriver();
	Cards = gefActivateGENIDevices (1,1,0);
	if (Cards>0) {
		printf("\nTurned on %u cards",Cards);
	}
	else {
		printf("\nError %d tuning on GENI cards",Cards);
	}
	getche();
	return(0);
}
#endif
/*
#define MAX_NON_CONFIG_SECTIONS 15
int NonConfigSectionCount=0;
long NonConfigSectionStart[MAX_NON_CONFIG_SECTIONS];
short NonConfigSectionLength[MAX_NON_CONFIG_SECTIONS];                                      
enum {ConfigSectionTypeRack=1,ConfigSectionTypeSBA,ConfigSectionTypePCIF,ConfigSectionTypeGENI,
		ConfigSectionTypeGBC,ConfigSectionTypePBC,ConfigSectionTypeFBC,
		ConfigSectionTypeDNC,ConfigSectionTypeHSC,ConfigSectionTypeAPM,
		ConfigSectionTypeDSM};
const char *pConfigSectionType[]={"RACK","SBA","PCIF","GENI","GBC","PBC","FBC","DNC","HSC","APM","DSM"};
*/
//int gefLoadConfigFile(char *pConfigFileName, int MaxModuleCount,
//		GEF_MODULE_CONFIG *pModuleConfigStart, GEF_GENI_CARD_CONFIG *pGENICardConfig)
int gefLoadGENIConfigFile(int GENICardNumber, char *pConfigFileName)
/*
*   Procedure to load user config (.CNF) file with racks and modules with
*   reference address for each module. I/O Point definitions can also be
*   included after each module with tagname, state labels or scale factor.
*   All rack, module, point or other .CNF lines can include a description.
*   Reference addresses are determined for all I/O points and are added to
*   the tagname list. This is called first and skips .Axis/.Counter/.Program
*	sections that are process on the second pass
*
*	This routine is called first to process all [Rack.Slot] or [Device.Slot] data with Part
*	and reference addresses. The ModuleID is read to valid what is actually installed.
*	The .Axis], .Program0] and .Counter] sections are skipped. Thye are porcessed later
*/  
{
	GEF_MODULE_CONFIG *pModuleConfig;
	GEF_GENI_CARD_CONFIG *pGENICardConfig;
	GEF_PLC_REF_TABLE_TYPES *pRefTableType;
	BYTE far *pStartRefTable[4];	// Start of I, Q, AI and AQ reference tables
	FILE *hInFile;
	GEF_PLC_TRANSFER_IO *pDeviceIO;
	int SBA,Slot,Loop,Letter; //Next,ByteLength,TypeCard,IntValue;
	int NumberBytes,ModuleListCount; //,Type,MaxType,CountMotionP;
	int Column,Numbers[5],NumberCount,Sign,ByteOffset,ByteLength;
	WORD HexNumber;
	char SaveLine[200],TextIn[120],*pText;    
#ifdef WANT_CIRCUITS
	GEF_PRODUCT_LIST far *pProductList;
	GEF_MODULE_LIST far *pModuleList;
	GEF_TAGNAME_LIST far *pTagNameList;
	GEF_IO_POINT_LIST *pIOPoint;
#endif
	int fPrintFile=FALSE;
	if ((GENICardNumber>0)&&(GENICardNumber<=4)) {
		pGENICardConfig = &gefGENICardConfig[GENICardNumber-1];
	}
	else {
		return(-1);
	}
	if (!pModuleConfigStart) {
		pModuleConfigStart = calloc(sizeof(GEF_MODULE_CONFIG),MaxModuleCount);
	}
	pRefTableType = gefRefTableTypes;
	for (Loop=0; Loop<4; Loop++) {
		pStartRefTable[Loop] = pRefTableType->pStartOfData;
		pRefTableType++;
	}
/*
*   Open config file with user modules and I/O point definitions
*/                                                 
	if (pConfigFileName) {
		hInFile = fopen(pConfigFileName,"r");
	}
	else {
		hInFile = fopen("TestPCIF.cfg","r");
	}
	if (!hInFile) {
		printf("Error - Can Not read config file %s",pConfigFileName);
		return(0);
	}   
	pModuleConfig = pModuleConfigStart;
	memset(pModuleConfig,0,sizeof(GEF_MODULE_CONFIG));
	ModuleListCount = 0;	
	memset(pGENICardConfig, 0, sizeof(GEF_GENI_CARD_CONFIG));
	do {
		pText = fgets(TextIn,sizeof(TextIn)-1,hInFile);
		NumberBytes = strlen(TextIn);                              
		Column = 0;
		NumberCount = 0;
		memset(Numbers,0,sizeof(Numbers));                    
		HexNumber = 0;
		Sign = 1;             
/*
*	Pack line as upper case and build numbers found up to first ; for command
*/
		for (Loop=0; Loop<NumberBytes; Loop++) {
			Letter = TextIn[Loop];
			if ((Letter>='a')&&(Letter<='z')) {
				Letter &= 0x5F;
			}                                                    
			if (Letter==';') {
				break;
			}
			if (NumberCount<5) {                        
				if ((Letter>='0')&&(Letter<='9')) {
					Numbers[NumberCount] *= 10;
					if (Sign>0) {
						Numbers[NumberCount] += Letter - '0';
					}
					else {
						Numbers[NumberCount] -= Letter - '0';
					}
					if (NumberCount==1) {
						HexNumber = (HexNumber<<4) | (Letter - '0');
					}
				}    
				if ((NumberCount==1)&&(Letter>='A')&&(Letter<='F')) {
					HexNumber = (HexNumber<<4) | (Letter - 'A' + 10);
				}
				if ((Letter=='.')||(Letter=='=')||(Letter==',')||(Letter=='(')) {
					NumberCount++;
					Sign = 1;
				}
				if (Letter=='-') {
					Sign = -1;
				}                     
			}
			if ((Column<sizeof(SaveLine))&&(Letter>' ')) {
				SaveLine[Column++] = (char) Letter;
			}
		}
		SaveLine[Column] = '\0';                          
//		printf("\n%s",SaveLine);
//		getch();
		if (SaveLine[0]=='[') {                               
			SBA = -1;
			Slot = 0;
			if (!memcmp(&SaveLine[1],"CARD",4)) {
				SBA = 32;
			}
			if (!memcmp(&SaveLine[1],"SBA",3)) {
				SBA = atoi(&SaveLine[4]);
				Letter = SaveLine[Column-2];
				if ((Letter>='1')&&(Letter<='8')) {
					Slot = Letter - '0';
				}
			}
			if (SBA>=0) {
				if (ModuleListCount<MaxModuleCount) {
					if (ModuleListCount) {
						pModuleConfig++;
					}
					ModuleListCount++;
				}
			}
#ifdef FUTURE_STUFF
			MaxType = sizeof(pConfigSectionType)/sizeof(pConfigSectionType[0]);
/*
*	Check for Section type, mostly for future control network support
*/
			for (Next=0; Next<MaxType; Next++) {
				ByteLength = strlen(pConfigSectionType[Next]);
				if (!memcmp(&SaveLine[1],pConfigSectionType[Next],ByteLength)) {
/*
*	If last part of the section name is .Axis or .Program, skip as not new
*	Base on length for now [Rack1.Slot10.Axis1] is over 15 bytes
*/                  if (Column>15) {
						break;
					}
					if ((CountModuleList>0)&&(CountModuleList<MaxModuleCount)) {
						pModuleConfig++;
						memset(pModuleConfig,0,sizeof(GEF_MODULE_CONFIG));
					}     
					pModuleConfig->TypeCard = TypeCard;
					Type = Next + 1;                             
					if (Type>ConfigSectionTypeRack) {
						pModuleConfig->RackDeviceSlot = (short)((Numbers[1]<<8) | Numbers[2]);
					}
					else {
						pModuleConfig->RackDeviceSlot = (short)((Numbers[0]<<8) | Numbers[1]);
					}                          
//printf("\nRack Type=%u RS=%Xh",Type,pModuleConfig->RackDeviceSlot);
					pModuleConfig->SectionStart = 0;
					pModuleConfig->SectionLength = 0;
					CountModuleList++;                
					break;
				}
			}
			if (Type) {
//				printf("\tSection[%u]=%s/%u,%u,%u",Type,SaveLine,Numbers[0],Numbers[1],Numbers[2]);
			}
			else {
				if (NonConfigSectionCount<MAX_NON_CONFIG_SECTIONS) {
					NonConfigSectionStart[NonConfigSectionCount] = 0;
					NonConfigSectionLength[NonConfigSectionCount] = 0;
					NonConfigSectionCount++;
				}
//				printf("\tOther section[%u]=%s",NonConfigSectionCount,SaveLine);
			}
#endif
		}
		else {
			if (SBA==32) {
				if (!memcmp(SaveLine,"PORTADDRESS",11)) {
					pGENICardConfig->PortAddress = HexNumber;
//					printf("\nPortAddress=%X",PortAddress);
				}
				if (!memcmp(SaveLine,"MEMORYSEGMENT",13)) {
					pGENICardConfig->pGENIMemory = (BYTE far *)(((long)HexNumber)<<16);
//					printf("\nMemorySegment=%X",MemorySegment);
				}
				if (!memcmp(SaveLine,"INTERRUPT",9)) {
					pGENICardConfig->Interrupt = Numbers[1];
//					printf("\nInterrupt=%u",Interrupt);
				}
			}
			if (SBA>=0) {
				if (!memcmp(SaveLine,"MODULEID",8)) {
					pModuleConfig->ModuleID = Numbers[1];
//					printf("\nControlByteLength=%u",ControlByteLength);
				}                          
//	Either Part or Product can be sued
				if (!memcmp(SaveLine,"PART",4)) {
					strcpy(pModuleConfig->PartNumber,&SaveLine[5]);                     
					SaveLine[0] = '\0';
				}
				if (!memcmp(SaveLine,"PRODUCT",7)) {
					strcpy(pModuleConfig->PartNumber,&SaveLine[8]);                     
					SaveLine[0] = '\0';
				}
				if (!memcmp(SaveLine,"REFERENCE",9)) {
					pDeviceIO = &pGENICardConfig->DeviceIO[SBA];
//				printf("\tReference %c%c = %u(%u)",SaveLine[9],SaveLine[10],Numbers[1],Numbers[2]);
					SaveLine[0] = '\0';
					ByteLength = (Numbers[2]+7)>>3;
					ByteOffset = (Numbers[1]-1)>>3;
					if (SaveLine[9]=='I') {
						pModuleConfig->AddressI = Numbers[1];
						pModuleConfig->LengthI = Numbers[2];
						pDeviceIO->ByteLengthI = ByteLength;
						pDeviceIO->pDataI = pStartRefTable[0];
						pDeviceIO->pDataI += ByteOffset;
						if (SaveLine[10]=='Q') {
							pModuleConfig->AddressQ = Numbers[1];
							pModuleConfig->LengthQ = Numbers[2];
							pDeviceIO->ByteLengthQ = ByteLength;
							pDeviceIO->pDataQ = pStartRefTable[1];
							pDeviceIO->pDataQ += ByteOffset;
						}
					}
					if (SaveLine[9]=='Q') {
						pModuleConfig->AddressQ = Numbers[1];
						pModuleConfig->LengthQ = Numbers[2];
						pDeviceIO->ByteLengthQ = ByteLength;
						pDeviceIO->pDataQ = pStartRefTable[1];
						pDeviceIO->pDataQ += ByteOffset;
						if (SaveLine[10]=='I') {
							pModuleConfig->AddressI = Numbers[1];
							pModuleConfig->LengthI = Numbers[2];
							pDeviceIO->ByteLengthI = ByteLength;
							pDeviceIO->pDataI = pStartRefTable[0];
							pDeviceIO->pDataI += ByteOffset;
						}
					}
					if (SaveLine[9]=='A') {
						ByteLength = Numbers[2]<<1;
						ByteOffset = (Numbers[1]-1)<<1;
						if (SaveLine[10]=='I') {
							pModuleConfig->AddressAI = Numbers[1] - 1;
							pModuleConfig->LengthAI = Numbers[2];
							pDeviceIO->ByteLengthAI = ByteLength;
							pDeviceIO->pDataAI = pStartRefTable[2];
							pDeviceIO->pDataAI += ByteOffset;
						}
						if (SaveLine[10]=='Q') {
							pModuleConfig->AddressAQ = Numbers[1] - 1;
							pModuleConfig->LengthAQ = Numbers[2];
							pDeviceIO->ByteLengthAQ = ByteLength;
							pDeviceIO->pDataAQ = pStartRefTable[3];
							pDeviceIO->pDataAQ += ByteOffset;
						}
					}
/* Drop Horner for GENI
					if (SaveLine[9]=='R') {
						if (SaveLine[10]=='I') {
							pModuleConfig->AddressRI = Numbers[1];
							pModuleConfig->LengthRI = Numbers[2];
						}
						if (SaveLine[10]=='Q') {
							pModuleConfig->AddressRQ = Numbers[1];
							pModuleConfig->LengthRQ = Numbers[2];
						}
					}
*/
					printf("\n%u(%u) %u(%u)/ %u(%u) %u(%u)",
						pModuleConfig->AddressI,pModuleConfig->LengthI,
						pModuleConfig->AddressAI,pModuleConfig->LengthAI,
						pModuleConfig->AddressQ,pModuleConfig->LengthQ,
						pModuleConfig->AddressAQ,pModuleConfig->LengthAQ);
				}	
/*
*	Standard ConfFigByteN=decimal value for foreign/Horner cards, N=1 to 12
*/
				if (!memcmp(SaveLine,"CONFIGBYTE",10)) {
					if ((Numbers[0]>0)&&(Numbers[0]<=12)) {
						pModuleConfig->ConfigData[Numbers[0]-1] = (BYTE)Numbers[1];
					}
					else {
						printf("\nError is %s, Byte limited 1 to 12",SaveLine);
					}
				}
			}
		}
//		Column = 0;
//		NumberCount = 0;
//		memset(Numbers,0,sizeof(Numbers));                    
//		Sign = 1;
	} while(pText);
//	NumberBytesRead = _filelength(hInFile);
	fclose(hInFile);
//	CountMotionP = gefLoadMotionData(pConfigFileName, CountModuleList,
//						1000,pModuleConfigStart);
//	printf("\nFound %u Motion P's",CountMotionP);
	return(ModuleListCount);
}
#ifdef DROP_MOTION
/*
*	Check for Key indicating a motion card. Assume 19200/Odd
*/
			if (!memcmp(SaveLine,"CONTROLFEEDBACK",15)) {
				IntValue = -1;
//				printf("\nControlFeedback");
				if (!memcmp(&SaveLine[16],"STANDARD/ENCODER",16)) {
					IntValue = 0;
				}
				if (!memcmp(&SaveLine[16],"STANDARD/RESOLVER",17)) {
					IntValue = 1;
				}
				if (!memcmp(&SaveLine[16],"STANDARD/DIGITAL",16)) {
					IntValue = 2;
				}
				if (!memcmp(&SaveLine[16],"STANDARD/LINEAR",15)) {
					IntValue = 4;
				}
				if (!memcmp(&SaveLine[16],"FOLLOWER/ENCODER",16)) {
					IntValue = 0x20;
				}
				if (!memcmp(&SaveLine[16],"FOLLOWER/DIGITAL",16)) {
					IntValue = 0x22;
				}                                      
/*
*	Set config data, Assume Digital isf Digital feedback, 19,200/Odd
*/
				if (IntValue>=0) {                          
					pModuleConfig->ConfigData[0] = 1;
					pModuleConfig->ConfigData[1] = (BYTE)IntValue;
					pModuleConfig->ConfigData[2] = 0; /* 19200/odd/1 */
					pModuleConfig->ConfigData[3] = 10; /* link idle */
					pModuleConfig->ConfigData[4] = 0; /* modem turn */
					memcpy(&pModuleConfig->ConfigData[5],"A00001",6);
					if ((IntValue%0xF)==2) {
						IntValue = 1;
					}                                
					else {
						IntValue = 0;
					}
					if (pModuleConfig->LengthAI<=40) {
						pModuleConfig->LengthAI = 40;
						pModuleConfig->LengthAQ = 6;
					}
					else {
						if (pModuleConfig->LengthAI>=64) {
							pModuleConfig->LengthAI = 64;
							pModuleConfig->LengthAQ = 12;
							IntValue |= 0x20;
						}
						else {
							pModuleConfig->LengthAI = 50;
							pModuleConfig->LengthAQ = 9;
							IntValue |= 0x10;
						}
					}
					pModuleConfig->ConfigData[11] = (BYTE)IntValue;
//					printf("=0=%X,10=%X",pModuleConfig->ConfigData[0],pModuleConfig->ConfigData[10]);
				}
			}
			
/*
*	Check for P parameters
*/
			if ((SaveLine[0]=='P')&&(SaveLine[1]>='0')&&(SaveLine[1]<='9')) {
				ParamNumber = atoi(&SaveLine[1]);
				pString = strchr(&SaveLine[1],'=');
				pDescription = NULL;
				if (pString) {
					ParamValue = atol(pString+1);
					pString = strchr(&SaveLine[1],',');
					if (pString) {
						pDescription = pString + 1;
					}
				}
//				printf(" P%u=%ld,%s",ParamNumber,ParamValue,pDescription);
				SaveLine[0] = '\0';
			}
#endif
#ifdef WIN32
//	structure and access routine for 32-bit systmes. The 16-bit systems access memory directly
typedef struct {
	WORD	DevicePresent[2];	// 32 device present bits, only updated on the read
	int		ByteLength[33];		// SBA 0 to 31 and global as 32, returns actual length
	BYTE	Data[4096+128];		// packed data to write to GENI or read back from GENI
} GEF_GENI_TRANSFER_DEVICEIO;
static GEF_GENI_TRANSFER_DEVICEIO TransferDeviceIO;
int gefTransferGENIDeviceIO(int GENICardNumber, int fWriteData)
/*
*	This is the 32-bit version for Windows that must use the GENICard driver to access hardware
*	The CommandTransferGENIMemory has not been added to the GENICard driver yet, so it has
*	been coded using Read/WriteGENIMemory calls for each serial bus address
*/
{
	int SBA,ByteLength,DataValue,Value,BitMask;
	WORD EnableOutputs[2];
	short ByteOffsetLength[2];
	GEF_GENI_CARD_CONFIG *pGENICardConfig;
	GEF_PLC_TRANSFER_IO *pDeviceIO;
	BYTE far *pMemory;

	if ((GENICardNumber>0)&&(GENICardNumber<=4)) {
		pGENICardConfig = &gefGENICardConfig[GENICardNumber-1];
		pDeviceIO = pGENICardConfig->DeviceIO;
	}
	else {
		return(-1);
	}
	pMemory = TransferDeviceIO.Data;
	if (fWriteData) {
		for (SBA=0; SBA<32; SBA++) {
			ByteLength = 0;
			if (pDeviceIO->pDataQ && pDeviceIO->ByteLengthQ) {
				ByteLength = pDeviceIO->ByteLengthQ;
				memcpy(pMemory,pDeviceIO->pDataQ,pDeviceIO->ByteLengthQ);
				pMemory += ByteLength;
			}
			if (pDeviceIO->pDataAQ && pDeviceIO->ByteLengthAQ) {
				ByteLength += pDeviceIO->ByteLengthAQ;
				memcpy(pMemory,pDeviceIO->pDataAQ,pDeviceIO->ByteLengthAQ);
				pMemory += pDeviceIO->ByteLengthAQ;
			}
			TransferDeviceIO.ByteLength[SBA] = ByteLength;
			pDeviceIO++;
		}
//	SBA=32, Q(32) Check if output enable bits have changed
		if (pDeviceIO->pDataQ && (pDeviceIO->ByteLengthQ==4)) {
			memcpy(EnableOutputs,pDeviceIO->pDataQ,4);
			if ((EnableOutputs[0]!=pGENICardConfig->EnableOutputs[0]) ||
				(EnableOutputs[1]!=pGENICardConfig->EnableOutputs[1])) {
				ByteOffsetLength[0] = 0x1E01;
				ByteOffsetLength[1] = 1;
				BitMask = 1;
				for (SBA=0; SBA<32; SBA++) {
					if (SBA<16) {
						Value = EnableOutputs[0]&BitMask;
					}
					else {
						if (SBA==16) {
							BitMask = 1;
						}
						Value = EnableOutputs[1]&BitMask;
					}
					BitMask <<= 1;
					DataValue = (Value ? 0 : 1);
					gefAccessGENIHardware(GENICardNumber,CommandWriteGENIMemory,ByteOffsetLength,&DataValue);
					ByteOffsetLength[0] += 8;
				}
				pGENICardConfig->EnableOutputs[0] = EnableOutputs[0];
				pGENICardConfig->EnableOutputs[1] = EnableOutputs[1];
			}
		}
		ByteLength = pDeviceIO->ByteLengthAQ;
		if (pDeviceIO->pDataAQ && ByteLength) {
			memcpy(pMemory,pDeviceIO->pDataAQ,ByteLength);
			pMemory += ByteLength;
		}
		TransferDeviceIO.ByteLength[32] = ByteLength;
//	Call GENICard driver to write data
//		gefAccessGENIHardware(GENICardNumber,CommandTransferGENIMemory,&TransferDeviceIO,NULL);
		ByteOffsetLength[0] = 0x3000;
		pMemory = TransferDeviceIO.Data;
		for (SBA=0; SBA<32; SBA++) {
			if (TransferDeviceIO.ByteLength[SBA]>0) {
				ByteOffsetLength[1] = TransferDeviceIO.ByteLength[SBA];
				gefAccessGENIHardware(GENICardNumber,CommandWriteGENIMemory,ByteOffsetLength,pMemory);
// DEBUG
printf("\nSBA=%u",SBA);
for (Value=0; Value<(int)ByteOffsetLength[1]; Value++) {
	printf(" %2X",*(pMemory+Value));
}
				pMemory += TransferDeviceIO.ByteLength[SBA];
			}
			ByteOffsetLength[0] += 128;
		}
		if (TransferDeviceIO.ByteLength[32]>0) {
			ByteOffsetLength[0] = 0x1F80;
			ByteOffsetLength[1] = TransferDeviceIO.ByteLength[32];
			gefAccessGENIHardware(GENICardNumber,CommandWriteGENIMemory,ByteOffsetLength,pMemory);
// DEBUG
printf("\nSBA=32");
for (Value=0; Value<(int)ByteOffsetLength[1]; Value++) {
	printf(" %2X",*(pMemory+Value));
}
		}
//	End of code to replace the CommandTransferGENIMemory
	}
	else {

		for (SBA=0; SBA<32; SBA++) {
			TransferDeviceIO.ByteLength[SBA] = pDeviceIO->ByteLengthI + pDeviceIO->ByteLengthAI;
			pDeviceIO++;
		}
		TransferDeviceIO.ByteLength[32] = pDeviceIO->ByteLengthAI;
//	Call GENICard driver to read data
//		gefAccessGENIHardware(GENICardNumber,CommandTransferGENIMemory,&TransferDeviceIO,&TransferDeviceIO);
		ByteOffsetLength[0] = 0x2000;
		pMemory = TransferDeviceIO.Data;
		for (SBA=0; SBA<32; SBA++) {
			if (TransferDeviceIO.ByteLength[SBA]>0) {
				ByteOffsetLength[1] = TransferDeviceIO.ByteLength[SBA];
				gefAccessGENIHardware(GENICardNumber,CommandReadGENIMemory,ByteOffsetLength,pMemory);
// DEBUG
memset(pMemory,SBA,TransferDeviceIO.ByteLength[SBA]);
				pMemory += TransferDeviceIO.ByteLength[SBA];
			}
			ByteOffsetLength[0] += 128;
		}
		TransferDeviceIO.DevicePresent[0] = 0;
		TransferDeviceIO.DevicePresent[1] = 0;
		ByteOffsetLength[0] = 0x1E02;
		ByteOffsetLength[1] = 1;
		DataValue = 0;
		BitMask = 1;
		for (SBA=0; SBA<32; SBA++) {
			gefAccessGENIHardware(GENICardNumber,CommandReadGENIMemory,ByteOffsetLength,&DataValue);
			if (DataValue) {
				if (SBA<16) {
					TransferDeviceIO.DevicePresent[0] |= BitMask;
				}
				else {
					if (SBA==16) {
						BitMask = 1;
					}
					TransferDeviceIO.DevicePresent[1] |= BitMask;
				}
			}
			BitMask <<= 1;
			ByteOffsetLength[0] += 8;
		}
//	End of code to replace the CommandTransferGENIMemory
		pDeviceIO = pGENICardConfig->DeviceIO;
		pMemory = TransferDeviceIO.Data;
		for (SBA=0; SBA<32; SBA++) {
			ByteLength = 0;
			if (pDeviceIO->pDataI && pDeviceIO->ByteLengthI) {
				ByteLength = pDeviceIO->ByteLengthI;
				memcpy(pDeviceIO->pDataI,pMemory,ByteLength);
				pMemory += ByteLength;
			}
			if (pDeviceIO->pDataAI && pDeviceIO->ByteLengthAI) {
				ByteLength += pDeviceIO->ByteLengthAI;
				memcpy(pDeviceIO->pDataAI,pMemory,pDeviceIO->ByteLengthAI);
				pMemory += pDeviceIO->ByteLengthAI;
			}
			if (TransferDeviceIO.ByteLength[SBA]!=ByteLength) {
				printf("\nError in input byte length Configured=%u Actual=%u",
					ByteLength, TransferDeviceIO.ByteLength[SBA]);
			}
			pDeviceIO++;
		}
//	SBA=32, I(32) return Device present bits
		if (pDeviceIO->pDataI && (pDeviceIO->ByteLengthI==4)) {
			memcpy(pDeviceIO->pDataI,TransferDeviceIO.DevicePresent,4);
		}
//	SBA=32, AI returns directed input data, if used
		if (pDeviceIO->pDataAI && pDeviceIO->ByteLengthAI) {
			ByteLength += pDeviceIO->ByteLengthAQ;
			memcpy(pDeviceIO->pDataAQ,pMemory,pDeviceIO->ByteLengthAI);
			pMemory += pDeviceIO->ByteLengthAI;
		}
	}
	ByteLength = (int)(pMemory - TransferDeviceIO.Data);
	return(ByteLength);
}
#else
int gefTransferGENIDeviceIO(int GENICardNumber, int fWriteData)
/*
*	This is the 16-bit version for DOS or the GENIGate microcontroller that can access
*	GENI memory directly without the GENICard driver
*	The GENIQuickMove code needs to be included for GENI memor interlocks
*/
{
	int SBA,ByteLength,BitMask,Value;
	WORD EnableOutputs[2],DevicePresent[2];
	GEF_GENI_CARD_CONFIG *pGENICardConfig;
	GEF_PLC_TRANSFER_IO *pDeviceIO;
	GENI_MEMORY far *pPCIM;
	GENI_DEVICE_BYTE far *pGENIDevice;
	BYTE far *pMemory;

	if ((GENICardNumber>0)&&(GENICardNumber<=4)) {
		pGENICardConfig = &gefGENICardConfig[GENICardNumber-1];
		pDeviceIO = pGENICardConfig->DeviceIO;
	}
	else {
		return(-1);
	}
	pPCIM = (GENI_MEMORY far *)pGENICardConfig->pGENIMemory;
//	NEED TO WAIT FOR GENI INTERLOCK HERE
	if (fWriteData) {
		pMemory = (BYTE far *)pPCIM->DeviceOutputTables;
		ByteLength = 0;
		for (SBA=0; SBA<32; SBA++) {
			if (pDeviceIO->pDataQ && pDeviceIO->ByteLengthQ) {
				ByteLength += pDeviceIO->ByteLengthQ;
				memcpy(pMemory,pDeviceIO->pDataQ,pDeviceIO->ByteLengthQ);
				pMemory += 128;
			}
			if (pDeviceIO->pDataAQ && pDeviceIO->ByteLengthAQ) {
				ByteLength += pDeviceIO->ByteLengthAQ;
				memcpy(pMemory,pDeviceIO->pDataAQ,pDeviceIO->ByteLengthAQ);
				pMemory += 128;
			}
			pDeviceIO++;
		}
//	SBA=32, Q(32) Check if output enable bits have changed
		if (pDeviceIO->pDataQ && (pDeviceIO->ByteLengthQ==4)) {
			memcpy(EnableOutputs,pDeviceIO->pDataQ,4);
			if ((EnableOutputs[0]!=pGENICardConfig->EnableOutputs[0]) ||
				(EnableOutputs[1]!=pGENICardConfig->EnableOutputs[1])) {
				pGENIDevice = pPCIM->Device;
				BitMask = 1;
				for (SBA=0; SBA<32; SBA++) {
					if (SBA<16) {
						Value = EnableOutputs[0]&BitMask;
					}
					else {
						if (SBA==16) {
							BitMask = 1;
						}
						Value = EnableOutputs[1]&BitMask;
					}
					BitMask <<= 1;
					pGENIDevice->OutputsDisabled = (BYTE)(Value ? 0 : 1);
					pGENIDevice++;
				}
				pGENICardConfig->EnableOutputs[0] = EnableOutputs[0];
				pGENICardConfig->EnableOutputs[1] = EnableOutputs[1];
			}
		}
		if (pDeviceIO->pDataAQ && pDeviceIO->ByteLengthAQ) {
			ByteLength += pDeviceIO->ByteLengthAQ;
			memcpy(pPCIM->BroadcastOutputTable,pDeviceIO->pDataAQ,ByteLength);
		}
	}
	else {
		pDeviceIO = pGENICardConfig->DeviceIO;
		pMemory = (BYTE far *)pPCIM->DeviceInputTables;
		ByteLength = 0;
		for (SBA=0; SBA<32; SBA++) {
			if (pDeviceIO->pDataI && pDeviceIO->ByteLengthI) {
				ByteLength += pDeviceIO->ByteLengthI;
				memcpy(pDeviceIO->pDataI,pMemory,pDeviceIO->ByteLengthI);
				pMemory += 128;
			}
			if (pDeviceIO->pDataAI && pDeviceIO->ByteLengthAI) {
				ByteLength += pDeviceIO->ByteLengthAI;
				memcpy(pDeviceIO->pDataAI,pMemory,pDeviceIO->ByteLengthAI);
				pMemory += 128;
			}
//			if (TransferDeviceIO.ByteLength[SBA]!=ByteLength) {
//				printf("\nError in input byte length Configured=%u Actual=%u",
//					ByteLength, TransferDeviceIO.ByteLength[SBA]);
//			}
			pDeviceIO++;
		}
//	SBA=32, I(32) return Device present bits
		if (pDeviceIO->pDataI && (pDeviceIO->ByteLengthI==4)) {
			pGENIDevice = pPCIM->Device;
			DevicePresent[0] = 0;
			DevicePresent[1] = 0;
			BitMask = 1;
			for (SBA=0; SBA<32; SBA++) {
				if (pGENIDevice->DevicePresent) {
					if (SBA<16) {
						DevicePresent[0] |= BitMask;
					}
					else {
						if (SBA==16) {
							BitMask = 1;
						}
						DevicePresent[1] |= BitMask;
					}
				}
				BitMask <<= 1;
				pGENIDevice++;
			}
			memcpy(pDeviceIO->pDataI,DevicePresent,4);
		}
//	SBA=32, AI returns directed input data, if used
		if (pDeviceIO->pDataAI && pDeviceIO->ByteLengthAI) {
			ByteLength += pDeviceIO->ByteLengthAI;
			memcpy(pDeviceIO->pDataAQ,pPCIM->DirectedInputTable,pDeviceIO->ByteLengthAI);
		}
	}
//	NEED TO RELEASE GENI INTERLOCK HERE
	return(ByteLength);
}
#endif
#ifdef FUTURE_PCIF_SUPPORT
int LoadPCIFCard(int PCIFCard, int ModuleCountStart, char *pConfigFileName, int StartMode)
/*
*	Routine to load a PICIF card from the config file and return the new
*	total module count added to the ModuleCount input at the start
(	StartMode is positive to reset the PCIF, set the address, load config 
*	data and startup in 1=Stop, 2=Pause or 3=Run mode.
*	If StarMode is 0 and the card appears to be already on, it only load
*	config data from the file, but does not store data to the card.
*/
{
	int Status;
	int PortAddress,ReloadCardConfig;
	WORD MemorySegment,WordValue;
	GEF_MODULE_CONFIG far *pModuleConfig;
	GEF_PCIF2_MEMORY far *pPCIF2;

	ReloadCardConfig = 0;
#ifdef WIN32    
/*
*	NT/9x will use the GENICard driver which handles I/O port and memory access
*/
	MemorySegment = 0xD000;
	WordValue = MemorySegment;
	PortAddress = 0x200;
#else
	MemorySegment = (WORD)gefLoadPCIFAddress(PCIFCard, pConfigFileName);
	PortAddress = MemorySegment & 0x3FF;
	MemorySegment &= 0xF800;
	printf("\nUsing config file %s.\nPause to define PCIF%u Memory=%Xh, I/O Port=%Xh",
					pConfigFileName,PCIFCard,MemorySegment,PortAddress);
	if (MemorySegment) {                                             
/*
*	Check if card is already on
*/
		pPCIF2 = (GEF_PCIF2_MEMORY far *)(((long)MemorySegment)<<16);
		WordValue = MemorySegment;
		if ((StartMode>0)||(pPCIF2->selft_test!=1)) {
			ReloadCardConfig = 1;
			gefConfigPIF400(PCIFCard, PortAddress, MemorySegment, 0);
			WordValue = gefConfigPIF400(PCIFCard, PortAddress, 0, 0);
		}                                    
		else {
			if (PCIFCard>0) {
				gefPCIFHardware[PCIFCard-1].pPCIF2 = pPCIF2;
			}
		}       
//		pPCIF2 = gefPCIFHardware[PCIFCard].pPCIF2;
	}
	else {
		gefConfigPIF300(PCIFCard, PortAddress);
		pPCIF2 = (GEF_PCIF2_MEMORY far *)NULL;
	}
#endif
//	ModuleCount = gefLoadInterfaceConfig(SaveLine);
	if (ModuleCount==0) {
		ModuleCount = 100;
		pModuleConfigStart = (GEF_MODULE_CONFIG *)calloc(ModuleCount,sizeof(GEF_MODULE_CONFIG));
	}
	else {       
		ModuleCount += 100;
		pModuleConfigStart = (GEF_MODULE_CONFIG *)realloc(pModuleConfigStart,ModuleCount*sizeof(GEF_MODULE_CONFIG));
    }                                                          
	Status = gefLoadConfigFile(pConfigFileName, ModuleCount,pModuleConfigStart);
	if ((Status>0)&&(Status<ModuleCount)) {
		ModuleCount = Status;
		pModuleConfigStart = (GEF_MODULE_CONFIG *)realloc(pModuleConfigStart,ModuleCount*sizeof(GEF_MODULE_CONFIG));
	}   
//	WordValue = MemorySegment;	//FORCE for debug
	if (ModuleCount>0) {                 
		pModuleConfig = pModuleConfigStart;
		pModuleConfig += ModuleCountStart;
		if (ReloadCardConfig>0) {
			if (WordValue==MemorySegment) {
				printf(" Memory set OK");
				gefStorePCIFConfig(PCIFCard, ModuleCount,pModuleConfig);
			}
			else {
				printf(" Read %X, Failed to set Memory",WordValue);
			}
		}    
		else {
			printf(" Card already configured");
		}
		ModuleCountStart += ModuleCount;            
		if (DebugTrace>0) {
			gefDumpConfigFile(SaveLine, PortAddress,MemorySegment, ModuleCount,pModuleConfig);
		}
		if (StartMode>0) {
			printf(" Set to mode=%Xh",StartMode);
			gefTimeDelay(55);
			gefSetPCIFState(PCIFCard,StartMode);
		}
		gefStoreMotionParameters(ModuleCount, pModuleConfigStart, ParamCount,pMotionParameterStart);
	}
	return(ModuleCountStart);
}	                                    
int WINAPI gefReadComputerMemory(int MemoryType, int PLCAddress, int DataLength, short far *pBuffer)
/*
*	Routine to read computer memory, unpacking discrete bits as 1 bit per 16-bit short integer
*	MemoryType is SNP Memory Type, such as 8 for %R, Symbols such as PLCMemoryTypeR can be used
*/
{
	int Loop,MemoryTypeLength,Address,Length,Value,BitMask;
	BYTE *pData;
	GEF_PLC_REF_TABLE_TYPES *pRefTable;

	MemoryTypeLength = sizeof(gefRefTableTypes)/sizeof(gefRefTableTypes[0]);
	pData = NULL;
//	Compare MemoryType to computer tables to get start of data for this type
	pRefTable = gefRefTableTypes;
	for (Loop=0; Loop<MemoryTypeLength; Loop++) {
		if ((MemoryType==pRefTable->SNPMemoryType)||(MemoryType==pRefTable->SNPMemoryTypePacked)) {
			pData = pRefTable->pStartOfData;
			break;
		}
		if (MemoryType==pRefTable->SNPMemoryTypeOverride) {
			pData = pRefTable->pStartOfOverrides;
			break;
		}
		pRefTable++;
	}
	if (!pData) {
		return(0);
	}
	Length = 0;
	Address = PLCAddress - 1;
//	Check if defined memory type, then check limits, cutting back length to fit
	if (pData && (Address>=0) && (DataLength>0)) {
		if (PLCAddress>pRefTable->HighestPLCAddress) {
			return(0);
		}
		Length = DataLength;
		if ((Address+Length)>pRefTable->HighestPLCAddress) {
			Length = pRefTable->HighestPLCAddress - Address;
		}
		if (MemoryType>PLCMemoryTypeAQ) {
//	Unpack discrete bits to short integer
			pData += Address>>3;
			BitMask = 1<<(Address&7);
			Value = *pData++;
			for (Loop=0; Loop<Length; Loop++) {
				*pBuffer++ = (short)((BitMask&Value) ? 1 : 0);
				BitMask <<= 1;
				if (BitMask>=0x100) {
					Value = *pData++;
					BitMask = 1;
				}
			}
		}
		else {
//	Copy word data directoy to return buffer
			pData += Address<<1;
			memcpy(pBuffer,pData,2*Length);
		}
	}
	return(Length);
}
int WINAPI gefWriteComputerMemory(int MemoryType, int PLCAddress, int DataLength, short far *pBuffer)
/*
*	Routine to write computer memory, packing discrete bits from 16-bit short integer with single bit.
*	MemoryType is SNP Memory Type, such as 8 for %R, Symbols such as PLCMemoryTypeR can be used
*	The write routine checks the discrete override tables and does not write to overrridden bits
*/
{
	int Loop,MemoryTypeLength,Address,Length,Value,Data,Override,BitMask;
	BYTE *pData,*pOverride;
	GEF_PLC_REF_TABLE_TYPES *pRefTable;

	MemoryTypeLength = sizeof(gefRefTableTypes)/sizeof(gefRefTableTypes[0]);
	pData = NULL;
//	Compare MemoryType to computer tables to get start of data for this type
	pRefTable = gefRefTableTypes;
	for (Loop=0; Loop<MemoryTypeLength; Loop++) {
		if ((MemoryType==pRefTable->SNPMemoryType)||(MemoryType==pRefTable->SNPMemoryTypePacked)) {
			pData = pRefTable->pStartOfData;
			pOverride = pRefTable->pStartOfOverrides;
			break;
		}
//	Treat override table as normal discrete without an override table
		if (MemoryType==pRefTable->SNPMemoryTypeOverride) {
			pData = pRefTable->pStartOfOverrides;
			pOverride = NULL;
			break;
		}
		pRefTable++;
	}
	if (!pData) {
		return(0);
	}
	Length = 0;
	Address = PLCAddress - 1;
//	Check if defined memory type, then check limits, cutting back length to fit
	if (pData && (Address>=0) && (DataLength>0)) {
		if (PLCAddress>pRefTable->HighestPLCAddress) {
			return(0);
		}
		Length = DataLength;
		if ((Address+Length)>pRefTable->HighestPLCAddress) {
			Length = pRefTable->HighestPLCAddress - Address;
		}
		if (MemoryType>PLCMemoryTypeAQ) {
//	Pack discrete bits from short integer. May have overide table to check also
			Override = 0;
			pData += Address>>3;
			Data = *pData;
			if (pOverride) {
				pOverride += Address>>3;
				Override = *pOverride++;
			}
			BitMask = 1<<(Address&7);
			for (Loop=0; Loop<Length; Loop++) {
//	Get next bit, if data not overridden and different, invert data bit in the table
				Value = *pBuffer++;
				if (Value) {
					Value = BitMask;
				}
				if (!(BitMask&Override)) {
					if (Value!=(BitMask&Data)) {
						*pData ^= BitMask;
					}
				}
//	Shift to next bit, increment byte address at byte boundary and override memory if available
				BitMask <<= 1;
				if (BitMask>=0x100) {
//					pData++;
					Data = *(++pData);
//					if (pOverride) {
//						pOverride++;
//						Override = *(++pOverride);
//					}
					BitMask = 1;
				}
			}
		}
		else {
//	Copy word data directoy from buffer
			pData += Address<<1;
			memcpy(pData, pBuffer,2*Length);
		}
	}
	return(Length);
}
int WINAPI gefEGDSuspend(int fSuspend)
/*
*	Routine to suspend (if fSuspend=TRUE) or resume (if fSuspend=FALSE) EGD data transfers
*	To prevent a user thread and the EGD thread from overriding each others data, you can
*	call this routine with TRUE to suspend the EGD thread, then call it again with FALSE
*	to resume EGD transfers. There is usually no conflict if you only read EGD data or write to
*	word EGD data being produced. If you write to Status, Config, MemoryLists or bit data,
*	you should use this routine to control thread access to data. Normal operation is 
*		gefEGDSuspend(TRUE);
*		gefWriteComputerMemory(Bit Data Types....  or any gefEGDxxxx(fWrite=TRUE, ...
*		gefEGDSuspend(FALSE);
*	Return status is 0 or the number of Millisec waiting for suspend for success or 
*	-1 if the EGD thread failed to go to suspend mode in 2 seconds.
*/
{
	int ReturnStatus;
	DWORD StartTime,DeltaTime;

	if (fSuspend) {
//	Tell EGD thread it must suspend transfers to or from data accessed by gefEGD routines
		gefEGDThreadData.Command = 1;
		ReturnStatus = -1;
		StartTime = GetTickCount();
//	Sleep so other thread can run to set Command to 2 indicating it has suspended transfers
		do {
			Sleep(0);
			DeltaTime = GetTickCount() - StartTime;
			if (gefEGDThreadData.Command>1) {
				ReturnStatus = (int)DeltaTime;
				break;
			}
		} while (DeltaTime<2000);
	}
	else {
//	No need to wait when resuming the EGD thread data transfers
		gefEGDThreadData.Command = 0;
		ReturnStatus = 0;
	}
	return(ReturnStatus);
}

int WINAPI gefEGDLoadConfig(DWORD DefaultHostTCPIP, int MaxExchangeCount)
/*
*	Rouinte to load the EGD exchange config data from the GEFComm.ini file 
*	The DefaultHostTCPIP address specifies host computer to load and if it is left at 0,
*	the current computer TCP/IP addresses is used. The MaxExchangeCount sets the upper limit
*	on the number of Producer and Consumer exchanges from this computer. 
*	Call with the MaxCount of 0 to stop the EGD thread and release all resources at exit
*/
{
	int Length,Letter,Loop,Pass,Column,EqualColumn,DeviceType;
	int InSectionLoading,LoadingSectionLength,ExchangeCount;
	long ProduceDeviceTypeNumber,LoadDeviceTypeNumber,CurrentDeviceTypeNumber;
	long PrimaryDeviceTypeNumber;
	int TCPIPCount,ConsumingThisLine;
	HOSTENT *pHostEntry;
	DWORD ProduceTCPIP,ConsumeTCPIP,ExchangeID;
	GEF_EGD_EXCHANGE_LIST *pEGDExchangeList;
	GEF_TCPIP_LIST *pTCPIPList, *pTCPIPListStart;
	char LoadingSection[12],FileName[_MAX_PATH];
	char *pText,*pTextStart;
	FILE *sFile;
#ifdef WIN32
    WSADATA wsaData;
#endif
	if (MaxExchangeCount<=0) {
//	close Ethernet sockets and shutdown thread
		gefEGDThreadData.Command = 3;
//	Wait up to 5 secnds for EGD thread to exit
		for (Loop=0; Loop<(5000/50); Loop++) {
			Sleep(50);
			if (gefEGDThreadData.Command>3) {
				break;
			}
		}
		pEGDExchangeList = gefEGDThreadData.pEGDExchangeListStart;
		for (Loop=0; Loop<gefEGDThreadData.ExchangeListCount; Loop++) {
			free(pEGDExchangeList->pAddressSegment);
			pEGDExchangeList->pAddressSegment = NULL;
			pEGDExchangeList++;
		}
		free(gefEGDThreadData.pEGDExchangeListStart);
		memset(&gefEGDThreadData,0,sizeof(gefEGDThreadData));
#ifdef WIN32
	WSACleanup();
#endif
		return(0);
	}
//	Initial setup, clear thread data and allocate memory for maximum specified exchanges
#ifdef WIN32
//	Setup Windows socket requesting v1.1 WinSock
    if (WSAStartup(MAKEWORD(1,1), &wsaData)) {
#ifdef _CONSOLE
		printf("\nTCP/IP network does not appear to be installed");
#endif
		return(0);
	}
#endif
	memset(&gefEGDThreadData,0,sizeof(gefEGDThreadData));
	if (!gefEGDThreadData.pEGDExchangeListStart) {
		gefEGDThreadData.pEGDExchangeListStart = (GEF_EGD_EXCHANGE_LIST *)calloc(MaxExchangeCount,sizeof(GEF_EGD_EXCHANGE_LIST));
		gefEGDThreadData.MaxExchangeListCount = MaxExchangeCount;
	}
//	If HostTCPIP is 0, get name and TCPIP of host computer
	if (DefaultHostTCPIP) {
		gefEGDThreadData.LoadTCPIP = DefaultHostTCPIP;
	}
	else {
		gethostname(FileName,sizeof(FileName));
		pHostEntry = gethostbyname(FileName);
		if (pHostEntry) {
			memcpy(&gefEGDThreadData.LoadTCPIP,pHostEntry->h_addr_list[0],sizeof(DWORD));
		}
		else {
			return(-2);
		}
	}
	pTCPIPListStart = _alloca(GEF_MAX_TCPIP_LIST*sizeof(GEF_TCPIP_LIST));
	TCPIPCount = 0;
	PrimaryDeviceTypeNumber = -1;
	ExchangeCount = 0;
	Length = GetCurrentDirectory(sizeof(FileName),FileName);
	memcpy(&FileName[Length],"\\GEFComm.ini",13);
	sFile = fopen(FileName,"rt");
	if (!sFile) {
		return(-1);
	}
	pTextStart = _alloca(GEF_MAX_INI_LINE_LENGTH);
//	Scan file to find section with TCPIP address matching name or number
	while (!feof(sFile)) {
		Length = 0;
		EqualColumn = 0;
		pText = pTextStart;
//	Get next line and convert to upper, dropping spaces and %, ; comments
		if (fgets(pText,GEF_MAX_INI_LINE_LENGTH, sFile)) {
			Column = 0;
			do {
				Letter = *(pTextStart+Column);
				if (Letter>='a') {
					Letter &= 0x5F;
				}	
				if (Letter==';') {
					break;
				}
				if (Letter>' ') {
					*pText++ = (char)Letter;
					Length++;
				}
				Column++;
			} while(Letter);
		}
		*pText = '\0';
		if (*pTextStart=='[') {
			memset(LoadingSection,0,sizeof(LoadingSection));
			LoadingSectionLength = Length - 2;
			memcpy(LoadingSection,pTextStart+1,LoadingSectionLength);
		}
		else {
			if (!memcmp(pTextStart,"TCPIP",5)) {
				ProduceTCPIP = inet_addr(pTextStart+6);
				if (ProduceTCPIP==INADDR_NONE) {
					pHostEntry = gethostbyname(pTextStart+6);
					if (pHostEntry) {
						memcpy(&ProduceTCPIP,pHostEntry->h_addr_list[0],sizeof(DWORD));
					}
				}
				if (ProduceTCPIP==gefEGDThreadData.LoadTCPIP) {
					LoadDeviceTypeNumber = gefParseDeviceTypeNumber(-1,LoadingSection);
					break;
				}
			}
		}
	}
	if (LoadDeviceTypeNumber<0) {
		return(-1);
	}
	pEGDExchangeList = gefEGDThreadData.pEGDExchangeListStart;
	memset(pEGDExchangeList,0,sizeof(GEF_EGD_EXCHANGE_LIST));
//	LengthDeviceType = (sizeof(pSectionTypes)/sizeof(pSectionTypes[0]));
//	Pass 0 Save all TCPi/IP addresses and Primary key in section being loaded
//	Pass 1 load Consume exchanges for Section loading or Primary
//	Pass 2 load Produce exchanges and match to Consume exchanges
	for (Pass=0; Pass<3; Pass++) {
		fseek(sFile,0, SEEK_SET);
		pTCPIPList = pTCPIPListStart;
		while (!feof(sFile)) {
			Length = 0;
			EqualColumn = 0;
			pText = pTextStart;
//	Get next line and convert to upper, dropping spaces and %, ; comments
			if (fgets(pText,GEF_MAX_INI_LINE_LENGTH, sFile)) {
				Column = 0;
//				SegmentCount = 0;
				do {
					Letter = *(pTextStart+Column);
					if (Letter>='a') {
						Letter &= 0x5F;
					}	
					if (Letter=='=') {
						EqualColumn = Length;
					}
					if (Letter==';') {
						break;
					}
//					if ((Letter=='.')||(Letter=='\t')) {
//						SegmentCount++;
//					}
					if ((Letter>' ')&&(Letter!='%')) {
						*pText++ = (char)Letter;
						Length++;
					}
					Column++;
				} while(Letter);
			}
			*pText = '\0';
			if (*pTextStart=='[') {
//	If section name, check if DeviceType and DeviceNumber and remember if 
				CurrentDeviceTypeNumber = gefParseDeviceTypeNumber(-1,pTextStart+1);
				InSectionLoading = FALSE;
				if (LoadDeviceTypeNumber==CurrentDeviceTypeNumber) {
					InSectionLoading = TRUE;
				}
			}
			else {
				if (Pass) {
					ExchangeID = 0;
					Letter = *(pTextStart + EqualColumn + 1);
					if (Pass>1) {
						if (!memcmp(pTextStart,"PRODUCE",7)) {
							ExchangeID = atoi(pTextStart+7);
							pEGDExchangeList = gefEGDThreadData.pEGDExchangeListStart;
							for (Loop=0; Loop<ExchangeCount; Loop++) {
								if ((pEGDExchangeList->ProducerDeviceTypeNumber==CurrentDeviceTypeNumber)&&
													(pEGDExchangeList->ExchangeID==ExchangeID)) {
									pText = pTextStart + EqualColumn + 1;
									gefScanAddressSegments(TRUE, gefEGDThreadData.LoadTCPIP, pText, pEGDExchangeList);
									break;
								}
								pEGDExchangeList++;
							}
						}
					}
					else {
						if (!memcmp(pTextStart,"CONSUME",7)) {
							ExchangeID = 0;
							ProduceDeviceTypeNumber = -1;
							for (Column=7; Column<EqualColumn; Column++) {
								Letter = *(pTextStart + Column);
								if ((Letter>='0')&&(Letter<='9')) {
									ExchangeID = 10*ExchangeID + Letter - '0';
								}
								else {
									ProduceDeviceTypeNumber = gefParseDeviceTypeNumber(-1,pTextStart+Column);
									break;
								}
							}
							if (ProduceDeviceTypeNumber>=0) {
								ProduceTCPIP = 0;
								ConsumeTCPIP = 0;
								ConsumingThisLine = InSectionLoading;
								if ((ProduceDeviceTypeNumber==LoadDeviceTypeNumber) ||
									(ProduceDeviceTypeNumber==PrimaryDeviceTypeNumber)) {
									ConsumingThisLine = TRUE;
								}
								if (((ProduceDeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE)>1) &&
									(CurrentDeviceTypeNumber==PrimaryDeviceTypeNumber)) {
									ConsumingThisLine = TRUE;
								}
								if (ConsumingThisLine) {
									DeviceType = ProduceDeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE;
									pTCPIPList = pTCPIPListStart;
									for (Loop=0; Loop<TCPIPCount; Loop++) {
										if (ProduceDeviceTypeNumber==pTCPIPList->DeviceTypeNumber) {
											ProduceTCPIP = pTCPIPList->TCPIP;
										}
										if (CurrentDeviceTypeNumber==pTCPIPList->DeviceTypeNumber) {
											ConsumeTCPIP = pTCPIPList->TCPIP;
										}
										pTCPIPList++;
									}
								}
								if (ProduceTCPIP && ConsumeTCPIP) {
									pEGDExchangeList->ExchangeID = ExchangeID;
									pEGDExchangeList->ProducerDeviceTypeNumber = ProduceDeviceTypeNumber;
									pEGDExchangeList->ProducerTCPIP = ProduceTCPIP;
									pEGDExchangeList->EnableExchange = 1;
									pEGDExchangeList->ConsumerDeviceTypeNumber = CurrentDeviceTypeNumber;
									pEGDExchangeList->ConsumerTCPIP = ConsumeTCPIP;
									pEGDExchangeList->PLCStatusTypeAddress = 0;
									pText = pTextStart + EqualColumn + 1;
									gefScanAddressSegments(FALSE, gefEGDThreadData.LoadTCPIP, pText, pEGDExchangeList);
									ExchangeCount++;
									pEGDExchangeList++;
									memset(pEGDExchangeList,0,sizeof(GEF_EGD_EXCHANGE_LIST));
								}
							}
						}
						if (!memcmp(pTextStart,"RECEIVE",7)) {
						}
					}
				}
				else {
//	Pass 0 saves TCP/IP addresses, checks for back controllers duplicating Primary exchanges
					if (!memcmp(pTextStart,"TCPIP",5)) {
						ProduceTCPIP = inet_addr(pTextStart+6);
						if (ProduceTCPIP==INADDR_NONE) {
//	Bad address, look up the computer name
							pHostEntry = gethostbyname(pTextStart+6);
							if (pHostEntry) {
								memcpy(&ProduceTCPIP,pHostEntry->h_addr_list[0],sizeof(DWORD));
							}
						}
						if (InSectionLoading) {
							gefEGDThreadData.LoadTCPIP = ProduceTCPIP;
						}
						if (ProduceTCPIP) {
							pTCPIPList->TCPIP = ProduceTCPIP;
							pTCPIPList->DeviceTypeNumber = CurrentDeviceTypeNumber;
							TCPIPCount++;
							pTCPIPList++;
						}
					}
					if (!memcmp(pTextStart,"PRIMARY",7)) {
						if (InSectionLoading) {
							PrimaryDeviceTypeNumber = gefParseDeviceTypeNumber(-1,pTextStart+8);
						}
					}
				}
			} // while not at end of file
		}
	}  // for Pass = 0 to 2
	gefEGDThreadData.ExchangeListCount = ExchangeCount;
//	_beginthread(gefCommProcessEGDThread,0,&gefEGDThreadData);
	return(ExchangeCount);
}
int WINAPI gefEGDStatus( int fWrite, int IndexBased0, GEF_EGD_EXCHANGE_STATUS *pExchangeStatus,
					   int TextBytes, char *pTextLine)
/*
*	Routine to write or return EGD status. fWrite is TRUE to write or FALSE to read status
*	IndexBase0 is index of the exchnage from 0 to one less than the defined exchanges. You
*	can stepp from Index from 0 until the return value is 0 indicated you are past the last one.
*	pExchangeStatus returns status, message counts, time till next transfer and active status.
*	By setting fWrite to TRUE, you can write back chnaged data.
*	TextBytes and pTextLine are optional entries to return the memory list as an ASCII string
*	for display. Set length to 0 or pointer to NULL to skip returning the string
*	It returned the number of bytes in the text string or 1 if no text or 0 if Index is too high
*/
{
	short DeviceType,DeviceNumber;
	int ReturnLength;
	struct in_addr TCPIPAddress;
	char *pStatusText,LocalText[120];
	GEF_EGD_EXCHANGE_LIST *pExchangeList;

	if (IndexBased0>=gefEGDThreadData.ExchangeListCount) {
		return(0);
	}
	pExchangeList = gefEGDThreadData.pEGDExchangeListStart;
	if (IndexBased0>0) {
		pExchangeList += IndexBased0;
	}
	ReturnLength = 1;
//	gefCommProcessEGDThread(&gefEGDThreadData);
	if (fWrite) {
//	DEBUG Need code to write back changed status data
	}
	else {
		memcpy(pExchangeStatus,pExchangeList,sizeof(GEF_EGD_EXCHANGE_STATUS));
	}
	if (pTextLine&&(TextBytes>0)) {
		DeviceType = (short)(pExchangeList->ProducerDeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE);
		DeviceNumber = (short)(pExchangeList->ProducerDeviceTypeNumber%GEF_SHIFT_DEVICE_TYPE);
		if (pExchangeStatus->ProducerTCPIP==gefEGDThreadData.LoadTCPIP) {
			pStatusText = (pExchangeStatus->EnableExchange ? "Enabled" : "Disabled");
		}
		else {
			pStatusText = "Consumed";
			if (pExchangeList->ConsumerTCPIP) {
				if (pExchangeList->DataBytesReceived) {
					if (!pExchangeStatus->EnableExchange) {
						pStatusText = "Disabled";
					}
				}
				else {
					pStatusText = "NoDataYet";
				}
			}
			else {
				pStatusText = "NotDefined";
			}
		}
		memcpy(&TCPIPAddress,&pExchangeStatus->ProducerTCPIP,sizeof(DWORD));
		ReturnLength = wsprintf(LocalText,"%s%u %s ID=%u  #%u Count=%u Errors=%u %u MS left, %s",
			pSectionTypes[DeviceType],DeviceNumber,inet_ntoa(TCPIPAddress),
			pExchangeStatus->ExchangeID,pExchangeStatus->RequestID, pExchangeStatus->ExchangeCount,
			pExchangeStatus->ErrorCount, pExchangeStatus->TimeTillTransferEGD, pStatusText);
			if (ReturnLength>=TextBytes) {
				ReturnLength = TextBytes - 1;
				LocalText[ReturnLength] = 0;
			}
			memcpy(pTextLine,LocalText,ReturnLength+1);
	}
	return(ReturnLength);
}
int WINAPI gefEGDConfig( int fWrite, int IndexBased0, GEF_EGD_EXCHANGE_CONFIG *pExchangeConfig,
						  int TextBytes, char *pTextLine)
/*
*	Routine to write or return EGD config. fWrite is TRUE to write or FALSE to read config
*	IndexBase0 is index of the exchange from 0 to one less than the defined exchanges. You
*	can stepp from Index from 0 until the return value is 0 indicated you are past the last one.
*	pExchangeConfig returns configurau
*	By setting fWrite to TRUE, you can write back changed config data for the exchange.
*	TextBytes and pTextLine are optional entries to return the memory list as an ASCII string
*	for display. Set length to 0 or pointer to NULL to skip returning the string
*	It returned the number of bytes in the text string or 1 if no text or 0 if Index is too high
*/
{
	short DeviceType,DeviceNumber;
	int ReturnLength,Type,Address;
	struct in_addr TCPIPAddress;
	char *pText,LocalText[120];
	GEF_EGD_EXCHANGE_LIST *pExchangeList;

	if (IndexBased0>=gefEGDThreadData.ExchangeListCount) {
		return(0);
	}
	pExchangeList = gefEGDThreadData.pEGDExchangeListStart;
	if (IndexBased0>0) {
		pExchangeList += IndexBased0;
	}
	ReturnLength = 1;
	if (fWrite) {
//	DEBUG Need code to write back changed config data
	}
	else {
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
		pExchangeConfig->DeviceType = DeviceType;
		pExchangeConfig->DeviceNumber = DeviceNumber;
		pExchangeConfig->ProducerPeriod = pExchangeList->ProducerPeriod;
		pExchangeConfig->ConsumerTimeout = pExchangeList->ConsumerTimeout;
		pExchangeConfig->DataByteLength = pExchangeList->DataByteLength;
		pExchangeConfig->AddressSegmentCount = pExchangeList->AddressSegmentCount;
		pExchangeConfig->PLCStatusTypeAddress = pExchangeList->PLCStatusTypeAddress;	
		pExchangeConfig->DataBytesReceived = pExchangeList->DataBytesReceived;
	}
	if (pTextLine&&(TextBytes>0)) {
		if (pExchangeConfig->ConsumerTCPIP) {
			memcpy(&TCPIPAddress,&pExchangeConfig->ConsumerTCPIP,sizeof(DWORD));
			ReturnLength = wsprintf(LocalText,"Produce ID=%u %s ",
				pExchangeConfig->ExchangeID,inet_ntoa(TCPIPAddress));
		}
		else {
			memcpy(&TCPIPAddress,&pExchangeConfig->ProducerTCPIP,sizeof(DWORD));
			ReturnLength = wsprintf(LocalText,"Consume %s ID=%u ",
				inet_ntoa(TCPIPAddress),pExchangeConfig->ExchangeID);
		}
		ReturnLength += wsprintf(&LocalText[ReturnLength],"%s%u Period/Timeout %u/%u %u areas %u/%u bytes",
			pSectionTypes[DeviceType],DeviceNumber,pExchangeConfig->ProducerPeriod,
			pExchangeConfig->ConsumerTimeout,pExchangeConfig->AddressSegmentCount,
			pExchangeConfig->DataByteLength,pExchangeConfig->DataBytesReceived);
		if (pExchangeConfig->PLCStatusTypeAddress>0) {
			Type = pExchangeConfig->PLCStatusTypeAddress>>16;
			pText = "R";
			if (Type==PLCMemoryTypeAI) {
				pText = "AI";
			}
			if (Type==PLCMemoryTypeAQ) {
				pText = "AQ";
			}
			Address = pExchangeConfig->PLCStatusTypeAddress&0x7FFF;
			ReturnLength += wsprintf(&LocalText[ReturnLength]," %%%s%u",pText,Address);
		}
		if (ReturnLength>=TextBytes) {
			ReturnLength = TextBytes - 1;
			LocalText[ReturnLength] = 0;
		}
		memcpy(pTextLine,LocalText,ReturnLength+1);
	}
	return(ReturnLength);
}
int WINAPI gefEGDMemoryList( int fWrite, GEF_EGD_EXCHANGE_CONFIG *pExchangeConfig, 
	int MemoryListCount, GEF_PLC_MEMORY_LIST *pMemoryListStart, int TextBytes, char *pTextLine)
/*
*	Routine to write or return memory lists. fWrite is TRUE to write or FALSE to read lists
*	pExchnage must point to an existing EGD (or fututr unsolicited Ethernet) exchange
*	MemoryCount is the number of segemnts to Write or the maximum number to read back
*	pMemoryListStart points to a list of memory segments with short Type, Address and Length
*	TextBytes and pTextLine are optional entries to return the memory list as an ASCII string
*	for display. Set length to 0 or pointer to NULL to skip returning the string
*	The routine returned the number of bytes in the text string or the segment count if no text.
*/
{
	int Loop,StoredSegmentCount,Segment,Type,MemoryTypeLength,DataLength,Length,ReturnLength;
	int Address,ByteLength,BitOffset,ByteOffset;
	char *pText;
	BYTE *pData,*pOverride;
	GEF_EGD_EXCHANGE_LIST *pExchangeList;
	GEF_EGD_ADDRESS_SEGMENT *pAddressSegment;
	GEF_PLC_MEMORY_LIST *pMemoryList;
	GEF_PLC_REF_TABLE_TYPES *pRefTable;

	ReturnLength = 0;
	pAddressSegment = NULL;
	StoredSegmentCount = 0;
	pExchangeList = gefEGDThreadData.pEGDExchangeListStart;
	pMemoryList = pMemoryListStart;
	MemoryTypeLength = sizeof(gefRefTableTypes)/sizeof(gefRefTableTypes[0]);
	for (Loop=0; Loop<gefEGDThreadData.ExchangeListCount; Loop++) {
		if ((pExchangeConfig->ProducerTCPIP==pExchangeList->ProducerTCPIP) &&
					(pExchangeConfig->ExchangeID==pExchangeList->ExchangeID)) {
			StoredSegmentCount = pExchangeList->AddressSegmentCount;
			pAddressSegment = pExchangeList->pAddressSegment;
			if (pMemoryListStart) {
				if (fWrite) {
					Length = MemoryListCount*sizeof(GEF_EGD_ADDRESS_SEGMENT);
					if (pExchangeList->pAddressSegment) {
						pExchangeList->pAddressSegment = realloc(pExchangeList->pAddressSegment, Length);
					}
					else {
						pExchangeList->pAddressSegment = malloc(Length);
					}
					pExchangeList->AddressSegmentCount = MemoryListCount;
					pAddressSegment = pExchangeList->pAddressSegment;
					ByteOffset = 0;
					for (Segment=0; Segment<MemoryListCount; Segment++) {
						memset(pAddressSegment,0,sizeof(GEF_EGD_ADDRESS_SEGMENT));
						pAddressSegment->SNPMemoryType = pMemoryList->SNPMemoryType;
						Address = pMemoryList->StartAddress;
						ByteLength = pMemoryList->DataLength;
						if (pAddressSegment->SNPMemoryType>=0) {
							Address = Address - 1;
							Length = pMemoryList->DataLength;
							if (pAddressSegment->SNPMemoryType>PLCMemoryTypeAQ) {
								BitOffset = Address&7;
								if (BitOffset) {
									Address -= BitOffset;
									Length += BitOffset;
								}
								ByteLength = (Length + 7)>>3;
							}
							else {
								ByteLength = Length<<1; 
							}
							pRefTable = gefRefTableTypes;
							for (Type=0; Type<MemoryTypeLength; Type++) {
								if (pMemoryList->SNPMemoryType==pRefTable->SNPMemoryType) {
									pData = pRefTable->pStartOfData;
									pOverride = pRefTable->pStartOfOverrides;
									if (pAddressSegment->SNPMemoryType>PLCMemoryTypeAQ) {
										pData += Address>>3;
										if (pOverride) {
											pOverride += Address>>3;
										}
									}
									else {
										pData += Address<<1;
									}
									pAddressSegment->pDataTable = pData;
									pAddressSegment->pOverrideTable = pOverride;
									break;
								}
								pRefTable++;
							}
						}
						pAddressSegment->StartAddress = (short)Address;
						pAddressSegment->DataByteLength = (short)ByteLength;
						pAddressSegment->ByteOffsetInMessage = (short)ByteOffset;
						ByteOffset += ByteLength;
						pAddressSegment++;
						pMemoryList++;
					}
					pExchangeList->DataByteLength = ByteOffset;
				}
				else {
//	Return existing memory list to called
					if (MemoryListCount>StoredSegmentCount) {
						MemoryListCount = StoredSegmentCount;
					}
					for (Segment=0; Segment<MemoryListCount; Segment++) {
						pMemoryList->SNPMemoryType = pAddressSegment->SNPMemoryType;
						if (pAddressSegment->SNPMemoryType>PLCMemoryTypeAQ) {
							DataLength = pAddressSegment->DataByteLength<<3;
						}
						else {
							DataLength = pAddressSegment->DataByteLength>>1;
						}
						pMemoryList->StartAddress = pAddressSegment->StartAddress;
						pMemoryList->DataLength = (short)DataLength;
						pMemoryList++;
						pAddressSegment++;
					}
				}
			}
			ReturnLength = MemoryListCount;
//	If called wants formatted text, pack memory type, address and (Length) for all segments
			if ((TextBytes>50)&&pTextLine) {
				ReturnLength = 0;
				pText = pTextLine;
				pAddressSegment = pExchangeList->pAddressSegment;
				for (Segment=0; Segment<MemoryListCount; Segment++) {
					if (pAddressSegment->SNPMemoryType>PLCMemoryTypeAQ) {
						DataLength = pAddressSegment->DataByteLength<<3;
					}
					else {
						DataLength = pAddressSegment->DataByteLength>>1;
					}
					pRefTable = gefRefTableTypes;
					for (Type=0; Type<MemoryTypeLength; Type++) {
						if (pAddressSegment->SNPMemoryType==pRefTable->SNPMemoryType) {
							if (DataLength>1) {
								Length = sprintf(pText,"%s%u(%u),",pRefTable->MemoryTypeLetters,
									pAddressSegment->StartAddress,DataLength);
							}
							else {
								Length = sprintf(pText,"%s%u,",pRefTable->MemoryTypeLetters,
									pAddressSegment->StartAddress);
							}
							pText += Length;
							ReturnLength += Length;
							break;
						}
						pRefTable++;
					}
					pAddressSegment++;
					if ((ReturnLength+20)>TextBytes) {
						break;
					}
				}
				if (ReturnLength>1) {
					ReturnLength--;
					pText--;
					*pText = '\0';
				}
			}
			break;
		}
		pExchangeList++;
	}
	return(ReturnLength);
}
static int WINAPI gefStoreEGDMessage(int fStore, GEF_EGD_EXCHANGE_LIST *pExchangeList, BYTE *pMessageDataStart)
/*
*	Routine to store or load EGD messages to the computer reference tables using the memory list
*	defined for the exchange. It also checks for overrides when storing data to the computer.
*/
{
	int ByteOffset,DataByteLength,DataByte,OldDataByte,OverrideByte;
	int Segment,SegmentCount,TotalDataBytes;
	BYTE *pData,*pOverrides,*pMessageData;
	GEF_EGD_ADDRESS_SEGMENT *pAddressSegment;

//	Store or Load EGD to computer reference tables based on defined memory list
	SegmentCount = pExchangeList->AddressSegmentCount;
	pAddressSegment = pExchangeList->pAddressSegment;
	TotalDataBytes = 0;
	if (pExchangeList->EnableExchange && pAddressSegment && (SegmentCount>0)) {
		ByteOffset = 0;
		for (Segment=0; Segment<SegmentCount; Segment++) {
//	The ByteOffset field to skip data is optional and only used if defined
			if (pAddressSegment->ByteOffsetInMessage) {
				ByteOffset = pAddressSegment->ByteOffsetInMessage;
			}
			DataByteLength = pAddressSegment->DataByteLength;
			pData = pAddressSegment->pDataTable;
			pOverrides = pAddressSegment->pOverrideTable;
			if ((pAddressSegment->SNPMemoryType>=0) && pData && (DataByteLength>0)) {
				pMessageData = pMessageDataStart;
				pMessageData += ByteOffset;
				if (fStore) {
					if (pOverrides) {
//	If override tables defined, transfer singles bytes and check override bits for each
						do {
							DataByte = *pMessageData++;
							ByteOffset++;
							TotalDataBytes++;
							OverrideByte = *pOverrides++;
							if (OverrideByte) {
								OldDataByte = *pData;
								DataByte = (OldDataByte&OverrideByte) | ((~OverrideByte)&DataByte);
							}
							*pData++ = (BYTE) DataByte;
						} while(--DataByteLength);
					}
					else {
						memcpy(pData, pMessageData,DataByteLength);
						ByteOffset += DataByteLength;
						TotalDataBytes += DataByteLength;
					}
				}
				else {
//	Loading message from computer reference tables does not check overrides
					memcpy(pMessageData, pData,DataByteLength);
					ByteOffset += DataByteLength;
					TotalDataBytes += DataByteLength;
				}
			}
		}
	}
	return(TotalDataBytes);
}
static void gefCommProcessEGDThread(GEF_EGD_THREAD_DATA *pThreadData)
{
	GEF_EGD_EXCHANGE_LIST *pExchangeList,*pSameTCPIPList;
	struct sockaddr_in HostSocketAddress,TargetSocketAddress,FromSocketAddress;
	int Loop,FoundExchangeInList,NotFirstCall;
    long ByteLength,DataLength,FromLength;
	DWORD LastTickMS,CurrentTickMS,DeltaTickMS;
	pThreadData->Command = 0;
//	pThreadData->pEGDExchangeListStart = pEGDExchangeListStart;
//	pThreadData->ExchangeListCount = ExchangeListCount;
//	pThreadData->LoadTCPIP = LoadTCPIP;
//	Create datagram sockets and bind to port "GF" to receive and send EGD messages
    pThreadData->HostSocket = socket(AF_INET, SOCK_DGRAM,0);
	pThreadData->TargetSocket = socket(AF_INET, SOCK_DGRAM,0);
	if ((pThreadData->HostSocket == INVALID_SOCKET)||(pThreadData->TargetSocket == INVALID_SOCKET)) {
		pThreadData->LastWinsockError = WSAGetLastError();
#ifdef _CONSOLE
		printf("\nError %d, Not able to create sockets. Check if TCP/IP networking installed",pThreadData->LastWinsockError);
#endif
		return;
	}
	else {
		memset(&HostSocketAddress,0,sizeof(HostSocketAddress));
		HostSocketAddress.sin_family = AF_INET;
		HostSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
		memcpy(&TargetSocketAddress,&HostSocketAddress,sizeof(HostSocketAddress));
		if (bind(pThreadData->HostSocket, (LPSOCKADDR)&HostSocketAddress, sizeof(HostSocketAddress)) == SOCKET_ERROR) {
			pThreadData->LastWinsockError = WSAGetLastError();
#ifdef _CONSOLE
			printf("\nError %d, Not able to bind this computer socket to EGD data port GE",pThreadData->LastWinsockError);
#endif
		}
	}
	NotFirstCall = FALSE;
	DeltaTickMS = 0;
	LastTickMS = 0;
// DEBUG  comment out endless loop so can be called once during debug
//	while (1) {
//	Check if any UDP datagrams on this port. If none, check EGD to send, then give up this tick
		do {
			if (pThreadData->Command) {
				break;
			}
			if (ioctlsocket (pThreadData->HostSocket, FIONREAD, &ByteLength)) {
				pThreadData->LastWinsockError = WSAGetLastError();
#ifdef _CONSOLE
				printf("Error %d checking socket with ioctl",pThreadData->LastWinsockError);
#endif
			}
			if (ByteLength>0) {
//	Received UDP datagram on this port
				FromLength = sizeof(FromSocketAddress);
			    ByteLength = recvfrom(pThreadData->HostSocket,(char *)&MessageEGD, sizeof(MessageEGD), 0, (LPSOCKADDR)&FromSocketAddress, &FromLength);
			    if (ByteLength == SOCKET_ERROR) {
					pThreadData->LastWinsockError = WSAGetLastError();
#ifdef _CONSOLE
					printf("Error %d receiving UDP datagram",pThreadData->LastWinsockError);
#endif
			    } 
				else {
					DataLength = ByteLength - 32;
//	Check data length and Type and version for EGD data
					if ((DataLength>0)&&(MessageEGD.PDUTypeVersion==0x010D)) {
						FoundExchangeInList = FALSE;
						pSameTCPIPList = NULL;
						pExchangeList = pThreadData->pEGDExchangeListStart;
						for (Loop=0; Loop<pThreadData->ExchangeListCount; Loop++) {
							if (MessageEGD.ProducerID==pExchangeList->ProducerTCPIP) {
								if (!pSameTCPIPList) {
									pSameTCPIPList = pExchangeList;
								}
								if (MessageEGD.ExchangeID==pExchangeList->ExchangeID) {
									FoundExchangeInList = TRUE;
									break;
								}
							}
							pExchangeList++;
						}
//	Add new consumer exchanges if room in list. Data is dropped as no memory list defined
						if (!FoundExchangeInList) {
							if (pThreadData->ExchangeListCount<pThreadData->MaxExchangeListCount) {
								pThreadData->ExchangeListCount++;
								FoundExchangeInList = TRUE;
								memset(pExchangeList,0,sizeof(GEF_EGD_EXCHANGE_LIST));
								pExchangeList->ProducerTCPIP = MessageEGD.ProducerID;
								pExchangeList->ExchangeID = MessageEGD.ExchangeID;
								if (pSameTCPIPList) {
// If another exchange had same TCP/IP address, copy some of its config data 
									pExchangeList->ProducerDeviceTypeNumber = pSameTCPIPList->ProducerDeviceTypeNumber;
									pExchangeList->ConsumerTCPIP = pSameTCPIPList->ConsumerTCPIP;
									pExchangeList->ConsumerDeviceTypeNumber = pSameTCPIPList->ConsumerDeviceTypeNumber;
									pExchangeList->ProducerPeriod = pSameTCPIPList->ProducerPeriod;
									pExchangeList->ConsumerTimeout = pSameTCPIPList->ConsumerTimeout;
								}
							}
						}
						if (FoundExchangeInList) {
							pExchangeList->ExchangeCount++;
							pExchangeList->DataBytesReceived = (short)DataLength;
							pExchangeList->RequestID = MessageEGD.RequestID;
							pExchangeList->TimeStampSec = MessageEGD.TimeStampSec;
							pExchangeList->TimeStampNanoSec = MessageEGD.TimeStampNanoSec;
							pExchangeList->TimeTillTransferEGD = pExchangeList->ConsumerTimeout;
							gefStoreEGDMessage(TRUE, pExchangeList, MessageEGD.ProductionData);
						}
					}
				}
			}
		} while(ByteLength>0);
//	GetTickCount resolution is 10 MS under NT/2000 or 55 MS under Windows 9x
//			CurrentTickMS = GetTickCount();
//	timeGetTime resolution defaults to 5 MS under NT/2000 or 1 MS under Windows 9x, needs winmm.lib
		CurrentTickMS = timeGetTime();	
		if (NotFirstCall) {
			DeltaTickMS = CurrentTickMS - LastTickMS;
		}
		else {
			NotFirstCall = TRUE;
		}
		LastTickMS = CurrentTickMS;
//	Check exchange list and decrement time to transfer, producing messages when necessary
		pExchangeList = pThreadData->pEGDExchangeListStart;
		for (Loop=0; Loop<pThreadData->ExchangeListCount; Loop++) {
			if (pThreadData->Command) {
				break;
			}
			if (pExchangeList->EnableExchange) {
				if (pExchangeList->TimeTillTransferEGD>(-3600000)) {
					pExchangeList->TimeTillTransferEGD -= (long)DeltaTickMS;
				}
				if ((pExchangeList->ProducerTCPIP==pThreadData->LoadTCPIP) &&
										(pExchangeList->TimeTillTransferEGD<=0)) {
					pExchangeList->RequestID++;
					MessageEGD.PDUTypeVersion = 0x010D;
					MessageEGD.RequestID = pExchangeList->RequestID;
					MessageEGD.ProducerID = pThreadData->LoadTCPIP;
					MessageEGD.ExchangeID = pExchangeList->ExchangeID;
					MessageEGD.TimeStampSec = time(NULL);
					MessageEGD.TimeStampNanoSec = 1000000*(CurrentTickMS%1000);
					MessageEGD.Status = 0;
					MessageEGD.ConfigSignature = 0;	
					MessageEGD.Reserved = 0;
					ByteLength = pExchangeList->DataByteLength + 32;
					if (ByteLength>32) {
						gefStoreEGDMessage(FALSE, pExchangeList, MessageEGD.ProductionData);
					}
					memcpy(&TargetSocketAddress.sin_addr,&pExchangeList->ConsumerTCPIP,sizeof(long));
					ByteLength = sendto(pThreadData->TargetSocket,(char *)&MessageEGD,ByteLength,0,(LPSOCKADDR)&TargetSocketAddress,sizeof(TargetSocketAddress));
					if (ByteLength == SOCKET_ERROR) {
						pThreadData->LastWinsockError = WSAGetLastError();
#ifdef _CONSOLE
						printf("Error %d sending UDP datagram",pThreadData->LastWinsockError);
#endif
					} 
					else {
						pExchangeList->TimeTillTransferEGD = pExchangeList->ProducerPeriod;
					}
				}
			}
			pExchangeList++;
		}
//	Check command if present
		if (pThreadData->Command) {
			if (pThreadData->Command==1) {
				pThreadData->Command = 2;
			}
			if (pThreadData->Command>=3) {
//				break;
			}
		}
		Sleep(0);
//	}
//	Release thread resources and indicate when done
    if (pThreadData->HostSocket) closesocket(pThreadData->HostSocket);
    if (pThreadData->TargetSocket) closesocket(pThreadData->TargetSocket);
	_endthread();
	pThreadData->Command = 4;
	return;
}
static int gefScanAddressSegments(int fProducerLine, DWORD LoadTCPIP, char *pTextLine, GEF_EGD_EXCHANGE_LIST *pExchangeList)
/*
*	Routine to build address segment list for producers and consumers. The list for I/O always
*	takes precedence of CPU or PLC lists as controllers may be redundant while I/O is not.
*	Ofter the CPU or PLC will only have a period or timeout followed by Status word location
*	for PLC's so their exchange list default the same I/O list as the I/O device.
*	For the CPU or PLC the producer list takes precedence of the consumer. 
*/
{
	int SegmentCount,Loop,Letter,Address,Length,FirstArrayLength,StartWithStatusWord;
	int Period,PLCStatusType,PLCStatusAddress,SecondLetter,MemoryLettersLength;
	int MemoryTypeLength,MemoryType,PackDataLength,ByteOffset,ByteLength,BitOffset;
	char *pText,*pTextSegment,MemoryTypeLetters[4],Text[240];
	BYTE *pData,*pOverride;
	GEF_EGD_ADDRESS_SEGMENT *pAddressSegment;
	GEF_PLC_REF_TABLE_TYPES *pRefTable;

	MemoryTypeLength = sizeof(gefRefTableTypes)/sizeof(gefRefTableTypes[0]);
//	Count commas and tabs to estimate number of segments, Also pack first number
	pText = pTextLine;
	pTextSegment = NULL;
	SegmentCount = 0;
	Period = 0;
	FirstArrayLength = 0;
	PLCStatusType = 0;
	PLCStatusAddress = 0;
	do {
		Letter = *pText++;
		if ((Letter==',')||(Letter=='\t')) {
			if (!pTextSegment) {
				pTextSegment = pText;
			}
			SegmentCount++;
		}
		if (SegmentCount) {
			if (!FirstArrayLength) {
//	Second field may be a PLCStatusType and PLCStatusAddress if single word in memory
				if (SegmentCount==1) {
					if ((Letter>='0')&&(Letter<='9')) {
						PLCStatusAddress = 10*PLCStatusAddress + Letter - '0';
					}
					else {
						if (Letter=='R') {
							PLCStatusType = PLCMemoryTypeR;
						}
						else {
							if (Letter=='A') {
								PLCStatusType = PLCMemoryTypeAI;
							}
							else {
								if ((Letter=='Q')&&(PLCStatusType==PLCMemoryTypeAI)) {
									PLCStatusType = PLCMemoryTypeAQ;
								}
							}
						}
					}
				}
				if ((Letter=='(')||(Letter=='[')) {
					FirstArrayLength = SegmentCount;
				}
			}
		}
		else {
//	First field has number with ProducerPeriod or ConsumerTimeout
			if ((Letter>='0')&&(Letter<='9')) {
				Period = 10*Period + Letter - '0';
			}
		}
	} while(Letter);
//	ConsumeTime and ProducerPeriod 30MS smaller on pass 1, then ProducerPeriod on pass 2
	Period = 10*((Period+9)/10);
	if (fProducerLine) {
		if (Period>=10) {
			pExchangeList->ProducerPeriod = Period;
		}
	}
	else {
		pExchangeList->ConsumerTimeout = Period;
		if (Period>=40) {
			Period -= 30;
		}
		else {
			Period = 10;
		}
		pExchangeList->ProducerPeriod = Period;
	}
	StartWithStatusWord = FALSE;
	if ((SegmentCount>0)&&(FirstArrayLength!=1)) {
		StartWithStatusWord = TRUE;
		pExchangeList->PLCStatusTypeAddress = (PLCStatusType<<16) | PLCStatusAddress;
//	Drop first single word if PLC Status word for exchange
		SegmentCount--;
		if (pTextSegment&&(SegmentCount>0)) {
			do {
				Letter = *pTextSegment++;
				if ((Letter==',')||(Letter=='\t')) {
					break;
				}
			} while(Letter);
		}
	}
	else {
		PLCStatusType = 0;
		PLCStatusAddress = 0;
		pExchangeList->PLCStatusTypeAddress = 0;
	}
//	Allocate memory for address segments, changing size if necessary
	if (SegmentCount>0) {
		Length = SegmentCount*sizeof(GEF_EGD_ADDRESS_SEGMENT);
		if (pExchangeList->pAddressSegment) {
			pExchangeList->pAddressSegment = realloc(pExchangeList->pAddressSegment, Length);
		}
		else {
			pExchangeList->pAddressSegment = malloc(Length);
		}
		pExchangeList->AddressSegmentCount = SegmentCount;
		SegmentCount = 0;
		pAddressSegment = pExchangeList->pAddressSegment;
		Address = 0;
		Length = 0;
		PackDataLength = FALSE;
		MemoryLettersLength = 0;
		ByteOffset = 0;
		do {
			Letter = *pTextSegment++;
			if ((Letter>='A')&&(Letter<='Z')&&(MemoryLettersLength<2)) {
				MemoryTypeLetters[MemoryLettersLength++] = (char)Letter;
			}
			if ((Letter>='0')&&(Letter<='9')) {
				if (PackDataLength) {
					Length = 10*Length + Letter - '0';
				}
				else {
					Address = 10*Address + Letter - '0';
				}
			}
			if ((Letter=='(')||(Letter=='[')) {
				PackDataLength = TRUE;
			}
			if ((Letter==',')||(Letter=='\t')||(!Letter)) {
				MemoryType = 0;
				MemoryTypeLetters[MemoryLettersLength] = '\0';
				SecondLetter = MemoryTypeLetters[1];
				if ((MemoryTypeLetters[0]=='G')&&(SecondLetter)) {
					if ((SecondLetter>='A')&&(SecondLetter<='E')) {
						Address += 1280*(SecondLetter&0x7);
					}
				}
				pRefTable = gefRefTableTypes;
				for (Loop=0; Loop<MemoryTypeLength; Loop++) {
					if (!strcmp(MemoryTypeLetters,pRefTable->MemoryTypeLetters)) {
						pAddressSegment->SNPMemoryType = pRefTable->SNPMemoryType;
						pData = pRefTable->pStartOfData;
						pOverride = pRefTable->pStartOfOverrides;
						if (Address<=0) {
							Address = 1;
						}
						if (Length<=0) {
							Length = 1;
						}
						if (pAddressSegment->SNPMemoryType>PLCMemoryTypeAQ) {
							BitOffset = (Address - 1)&7;
							if (BitOffset) {
								Address -= BitOffset;
								Length += BitOffset;
							}
							ByteLength = (Length + 7)>>3;
							pData += Address>>3;
							if (pOverride) {
								pOverride += Address>>3;
							}
						}
						else {
							ByteLength = Length<<1; 
							pData += (Address-1)<<1;
						}
						pAddressSegment->StartAddress = (short)Address;
						pAddressSegment->DataByteLength = (short)ByteLength;
						pAddressSegment->pDataTable = pData;
						pAddressSegment->pOverrideTable = pOverride;
						pAddressSegment->ByteOffsetInMessage = (short)ByteOffset;
						ByteOffset += ByteLength;
						SegmentCount++;
						pAddressSegment++;
						Address = 0;
						Length = 0;
						PackDataLength = FALSE;
						MemoryLettersLength = 0;
						break;
					}
					pRefTable++;
				}
			}
		} while(Letter);
		pExchangeList->DataByteLength = ByteOffset;
//	Debug, warn if segment count differt than allocated space
		if (pExchangeList->AddressSegmentCount!=SegmentCount) {
			wsprintf(Text,"Counted %u segments and %u commas in line\n%s",
				pExchangeList->AddressSegmentCount,SegmentCount,pTextLine);
			MessageBox(NULL,Text,"Incorrect Segment Count",MB_OK);
		}
	}
	return(SegmentCount);
}
static long WINAPI gefParseDeviceTypeNumber(long DeviceTypeNumber, char *pDeviceTypeNumber)
/*
*	Routine to parse a device Type and number string, such as "PLC4" if DeviceTypeNumber
*	is < 0 and return a long value with type in high part and number in low part.
*	The Type has to be 3 letters (upper or lower case) that is in the list.
*	The Number is following digits and will be terminated with any non-digit
*	The high part is multipled by GEF_SHIFT_DEVICE_TYPE 
*	If the Type is not recognized, -1 is returned
*	If DeviceTypeNumber is >= 0, it is used to build string with 3 letters, then digits
*/
{
	int Letter,LengthDeviceType,Loop;
	char *pText,TypeString[3];
	long ReturnTypeNumber,Number;
	
	LengthDeviceType = (sizeof(pSectionTypes)/sizeof(pSectionTypes[0]));
	if (DeviceTypeNumber>=0) {
//	If DeviceType and DeviceNumber provided, use to build string
		Loop = DeviceTypeNumber/GEF_SHIFT_DEVICE_TYPE;
		Number = DeviceTypeNumber%GEF_SHIFT_DEVICE_TYPE;
		if ((Loop<0)||(Loop>=LengthDeviceType)) {
			Loop = 0;
		}
		memcpy(pDeviceTypeNumber,pSectionTypes[Loop],3);
		itoa(Number,pDeviceTypeNumber+3,10);
		return(DeviceTypeNumber);
	}
//	Otherwise parse string provided to return DeviceType and DeviceNumber
	ReturnTypeNumber = -1;
	pText = pDeviceTypeNumber;
	if (pText) {
		for (Loop=0; Loop<3; Loop++) {
			Letter = *pText++;
			if (Letter>='a') {
				Letter &= 0x5F;
			}
			TypeString[Loop] = (char)Letter;
		}
		for (Loop=0; Loop<LengthDeviceType; Loop++) {
			if (!memcmp(TypeString,pSectionTypes[Loop],3)) {
				Number = 0;
				do {
					Letter = *pText++;
					if ((Letter>='0')&&(Letter<='9')) {
						Number = 10*Number + Letter - '0';
					}
					else {
						break;
					}
				} while (Number<(GEF_SHIFT_DEVICE_TYPE/10));
				ReturnTypeNumber = Loop*GEF_SHIFT_DEVICE_TYPE + Number;
				break;
			}
		}
	}
	return(ReturnTypeNumber);
}
#endif