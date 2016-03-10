/*
*	The HostEGD program is a simple console mode program to show calls to the CommEGD
*	routines in the CommEGD.c file. It used the CommEGD.h header file for now, but this
*	will be moving into the GEFComm.h file in the future. 
*
*	This program uses the gefEGDLoadConfig to load the GEFComm.ini file that defines all
*	EGD exchanges as well as other PLC communications setup. The routines to access memory are
*		gefReadComputerMemory to read from computer reference tables
*		gefWriteComputerMemory to write to computer reference tables
*	To view or change EGD exchange configuration, you can call the following routines
*		gefEGDStatus	To get EGD status or reset error count or Enable Exchange
*		gefEGDConfig	To get config or change period, TCP/IP addresses or memory list size
*		gefEGDMemoryList To get or change memory list for specified EGD exchange config
*		gefEGDSuspend	To suspend EGD thread access to internal data while making changes
*
*	This program has been developed by Doug MacLeod at GE Fanuc PLC Tech Support
*	To create a project with MS VisualC 5/6, create a new project as a win32 console app
*	and change the project/settings/link Object/library modules list to include wsock32.lib
*	Also add winmm.lib to use the high resolution timer routine timeGetTime().
*	Go to the C/C++/Category Code Generation
*
*	You can include the GENIGate code into this program to add Genius PCIM support on this
*	computer. Edit the CommEGD.h file to uncomment the #define for the INCLUDE_GENIGATE symbol.
*	Include the GENIGate.c code along with GEFComm.lib. GEFComm.dll must be in the system path.
*/ 
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "CommEGD.h"
#define MAX_PLC_ADDRESS 16384
#define MAX_TEXT_BUFFER 32000
int WINAPI gefParseInputValues(char *pValueString, GEF_PLC_MEMORY_LIST *pMemoryList,
					int MaxValues, short *pValues);
int WINAPI gefPackRefTableText(GEF_PLC_MEMORY_LIST *pMemoryList, short *pData,
							   int MaxText, char *pTextStart);
char *pHelpText={"The HostEGD program loads EGD exchange definitions from the GEFComm.ini file\n"
				"and starts EGD transfers from and to computer memory. You can enter memory\n"
				"addresses and lengths in the format Address(Length), like I33(64), AI3(10) or\n"
				"R1(500) to display data in PLC reference table format. You can write data to\n"
				"addresses by entering values after an = sign. Example AQ1(3)=1,2,3 or AQ1=1,2,3\n"
				"sets 3 analog outputs. For bits, Q17(8)=10001110 or Q17=10001110 sets 8 bits.\n"
				"If Length in ()'s is bigger than the value count, last value fills the array.\n"
				"For example, AQ1(25)=0 clears 25 analog outputs or Q33(16)=1 sets 16 bits.\n"
				"\nTo view all data transferred in an EGD exchange, enter exchange number from 0\n"
				"to the highest exchange minus 1. Exchange config, status and data is displayed.\n"
				"\nEnter X or x to Exit the program."};
int main(int argc, char *argv[])
{
	char Text[256],*pTextStart;
	GEF_EGD_EXCHANGE_CONFIG ExchangeConfig;
	GEF_EGD_EXCHANGE_STATUS ExchangeStatus;
	GEF_PLC_MEMORY_LIST *pMemoryListStart,*pMemoryList,MemoryList;
	int Loop,Letter,Length,ExchangeCount,MemorySegments,ValuesToWrite;
	short *pDataStart;

//	Allocate space for memory lists and data and load EGD config data.(up to 256 segments)
	pMemoryListStart = _alloca(sizeof(GEF_PLC_MEMORY_LIST)*256);
	pDataStart = _alloca(MAX_PLC_ADDRESS*sizeof(short));
	pTextStart = _alloca(MAX_TEXT_BUFFER);
	SetConsoleTitle("HostEGD");
	ExchangeCount = gefEGDLoadConfig(0, 100);
	if (ExchangeCount<=0) {
		printf("\nError, Loading EGD Config returned %d\nMake sure GEFComm.ini has TCPIP=This computer address or name",ExchangeCount);
		return(1);
	}
//	Display list on EGD exchanges defined in the file
	for (Loop=0; Loop<ExchangeCount; Loop++) {
		if (gefEGDConfig( FALSE, Loop, &ExchangeConfig, MAX_TEXT_BUFFER, pTextStart)>0) {
			printf("\n%u=%s",Loop,pTextStart);
			MemorySegments = ExchangeConfig.MemoryListCount;
			if (MemorySegments>0) {
				gefEGDMemoryList( FALSE, &ExchangeConfig, MemorySegments,pMemoryListStart,MAX_TEXT_BUFFER, pTextStart);
				printf("\n\t%s",pTextStart);
			}
		}
	}
//	Prompt for commands until user enters a Q to quit the program
	do {
#ifdef INCLUDE_GENIGATE
//	Write configured outputs and read inputs for GENI1
		gefTransferGENIDeviceIO(1, TRUE);
		gefTransferGENIDeviceIO(1, FALSE);
#endif
		printf("\nCommand or ?->");
		gets(Text);
		strupr(Text);
		Letter = Text[0];
		if ((Letter>='0')&&(Letter<='9')) {
			Loop = atoi(Text);			
			Length = gefEGDStatus( FALSE, Loop, &ExchangeStatus, MAX_TEXT_BUFFER, pTextStart);
			if (Length>0) {
				printf("%s",pTextStart);
				gefEGDConfig( FALSE, Loop, &ExchangeConfig, MAX_TEXT_BUFFER, pTextStart);
				printf("\n%s",pTextStart);
				MemorySegments = ExchangeConfig.MemoryListCount;
				if (MemorySegments>0) {
					gefEGDMemoryList( FALSE, &ExchangeConfig, MemorySegments,pMemoryListStart,MAX_TEXT_BUFFER, pTextStart);
					printf("\n\t%s",pTextStart);
					pMemoryList = pMemoryListStart;
					for (Loop=0; Loop<MemorySegments; Loop++) {
						gefReadComputerMemory(pMemoryList->SNPMemoryType, pMemoryList->StartAddress,
								pMemoryList->DataLength, pDataStart);
						gefPackRefTableText(pMemoryList, pDataStart,MAX_TEXT_BUFFER, pTextStart);
						printf("\n%s",pTextStart);
						pMemoryList++;
					}
				}
			}
			else {
				printf("Enter Exchange Index from 0 to %u or ? for Help. Enter X to exit",ExchangeCount-1);
			}
		}
		else {
			ValuesToWrite = gefParseInputValues(Text, &MemoryList, MAX_PLC_ADDRESS, pDataStart);
			if (MemoryList.SNPMemoryType>=PLCMemoryTypeR) {
				if (ValuesToWrite>0) {
					gefWriteComputerMemory(MemoryList.SNPMemoryType, MemoryList.StartAddress,
						MemoryList.DataLength, pDataStart);
				}
				gefReadComputerMemory(MemoryList.SNPMemoryType, MemoryList.StartAddress,
					MemoryList.DataLength, pDataStart);
				gefPackRefTableText(&MemoryList, pDataStart,MAX_TEXT_BUFFER, pTextStart);
				printf("%s",pTextStart);
			}
			else {
				if (Letter!='X') {
					printf("%s",pHelpText);
				}
			}
		}
	} while(Letter!='X');
	return(0);
}
static char *pMemoryLetters[]={"I","Q","AI","AQ","R","M","G","T","S","SA","SB","SC"};
static short MemoryTypes[]={PLCMemoryTypeI,PLCMemoryTypeQ,PLCMemoryTypeAI,PLCMemoryTypeAQ,
		PLCMemoryTypeR,PLCMemoryTypeM,PLCMemoryTypeG,PLCMemoryTypeT,PLCMemoryTypeS,
		PLCMemoryTypeSA,PLCMemoryTypeSB,PLCMemoryTypeSC};
int WINAPI gefPackRefTableText(GEF_PLC_MEMORY_LIST *pMemoryList, short *pData,
							   int MaxText, char *pTextStart)
/*
*	Routine to convert block of memory to reference table text format. The pMemoryList
*	defines the starting address and data length for data in pData. Discrete data is
*	stored as 1 bit per 16-bit word to match gefReadPLCMemory. Text is packed in pTextStart
*	up to MaxText bytes including a null termination character. The fuction returns the
*	number of bytes packed in pTextStart
*/
{
	int Loop, ByteCount,LetterCount,Address,DataLength,Value,Column,Digit;
	char Letters[8],*pText,Line[84];

// Locate memory type letters, return if unknown memory type
	LetterCount = 0;
	for (Loop=0; Loop<(sizeof(MemoryTypes)/sizeof(MemoryTypes[0])); Loop++) {
		if (pMemoryList->SNPMemoryType==MemoryTypes[Loop]) {
			LetterCount = strlen(pMemoryLetters[Loop]);
			memcpy(Letters,pMemoryLetters[Loop],LetterCount);
			break;
		}
	}
	if (!LetterCount) {
		return(0);
	}
	pText = pTextStart;
	Address = pMemoryList->StartAddress;
	DataLength = pMemoryList->DataLength;
	ByteCount = 0;
	do {
		memset(Line,' ',sizeof(Line));
		Line[79] = '\n';
		memcpy(Line,Letters,LetterCount);
		Value = Address;
		for (Digit=4; Digit>=0; Digit--) {
			Line[LetterCount+Digit] = (Value%10) + '0';
			Value /= 10;
		}
		Column = 8;
		if (pMemoryList->SNPMemoryType>PLCMemoryTypeAQ) {
//	Display discrete as 8 bit bytes, 8 bytes per line
			for (Loop=0; Loop<64; Loop++) {
				if ((Loop>0)&&(!(Loop%8))) {
					Column++;
				}
				Value = *pData++;
				Address++;
				Line[Column++] = (char)(Value ? '1' : '0');
				if (--DataLength<=0) {
					break;
				}
			}
		}
		else {
//	Display word data as 16-bit signed integer with 5 digits
			for (Loop=0; Loop<10; Loop++) {
				Value = *pData++;
				if (Value>=0) {
					Line[Column] = '+';
				}
				else {
					Line[Column] = '-';
					Value = -Value;
				}
				for (Digit=5; Digit>0; Digit--) {
					Line[Column+Digit] = (Value%10) + '0';
					Value /= 10;
				}
				Column += 7;
				Address++;
				if (--DataLength<=0) {
					break;
				}
			}
		}
		if (DataLength>0) {
			Column = 80;
		}
//		else {
//			Line[Column++] = '\0';
//		}
//	Add line to text buffer, quitting if there is no more room
		if ((ByteCount+Column)>=MaxText) {
			Column = MaxText - ByteCount;
			DataLength = 0;
		}
		memcpy(pText,Line,Column);
		pText += Column;
		ByteCount += Column;
	} while(DataLength>0);
	*pText = '\0';
	return(ByteCount);
}
int WINAPI gefParseInputValues(char *pValueString, GEF_PLC_MEMORY_LIST *pMemoryList,
					int MaxValues, short *pValues)
/*
*	Routine to parse a string starting with a PLC address followed by data values.
*	pValueString has PLC Address(Length) = Data values as a string of 0's and 1' for discrete
*	or word values separated by spaces, commas or tabs. The Length in ()'s is options as
*	it is determined by the number of values. The Address does not need a % and either letters
*	or digits can be plcased firsy. such as R25 = 1,2,3,4,5,6  ot 123M = 100010100010101.
*	If (Length) is bigger than number of values entered, the array is fillwed with the last value
*	like R23(50) = 23,34 will return R23=23 and R24 to R82=34. R1(1000)=0 sets 1000 R's to 0. 
*	Parse input string and return up to MaxValues in 16 bit array pValues
*	Function returns the number of short Values stored, up to MaxValues, or a 0 if no values
*	were entered after the address.
*/
{
	char Letters[64]; 
	int Loop,Letter,Address,Length; 
	int Field,Index,Separator,LetterCount; 
	short ShortValue;

	Address = 0;
	Length = 0;
	LetterCount = 0;
	Field = 0;
	Index = 0;
	do {
		Letter = *pValueString++;
		if (Letter>='a') {
			Letter &= 0x5F;
		}
		Separator = FALSE;
//	Check for field terminators
		if ((Letter=='=')||(Letter==',')||(Letter<=' ')) {
			Separator = TRUE;
			if (Field<2) {
				Letters[LetterCount] = '\0';
				pMemoryList->SNPMemoryType = 0;
				for (Loop=0; Loop<(sizeof(MemoryTypes)/sizeof(MemoryTypes[0])); Loop++) {
					if (!strcmp(Letters,pMemoryLetters[Loop])) {
						pMemoryList->SNPMemoryType = MemoryTypes[Loop];
						break;
					}
				}
				LetterCount = 0;
				Field = 2;
			}
		}
		if (Field<2) {
			if (Letter=='(') {
				Field = 1;
			}
			if ((Letter>='0')&&(Letter<='9')) {
				if (Field) {
					Length = 10*Length + Letter - '0';
					if (Length>MaxValues) {
						Length = MaxValues;
					}
				}
				else {
					Address = 10*Address + Letter - '0';
				}
			}
			if ((Letter>='A')&&(Letter<='Z')&&(LetterCount<3)) {
				Letters[LetterCount++] = (char)Letter;
			}
		}
		else {
			if (pMemoryList->SNPMemoryType>PLCMemoryTypeAQ) {
				if ((Letter=='0')||(Letter=='1')) {
					*pValues++ = (short)(Letter&1);
					Index++;
				}
			}
			else {
				if (Separator) {
					Letters[LetterCount] = '\0';
					if (LetterCount>0) {
						*pValues++ = (short)atoi(Letters);
						Index++;
					}
					LetterCount = 0;
				}
				else {
					if ((Letter>' ')&&(LetterCount<(sizeof(Letters)-1))) {
						Letters[LetterCount++] = (char)Letter;
					}
				}
			}
		}
		if (Index>=MaxValues) {
			break;
		}
	} while(Letter);
//	Check address and compare (Length) entered to the last value Index
	if (Address<=0) {
		Address = 1;
	}
	if (Index>Length) {
		Length = Index;
	}
	if ((Index>0)&&(Index<Length)) {
		ShortValue = *(pValues-1);
		do {
			*pValues++ = ShortValue;
			Index++;
		} while (Index<Length);
	}
//	Return Address and Length entered or calculated. Return highest value Index which is 0 for Read
	pMemoryList->StartAddress = (short)Address;
	pMemoryList->DataLength = (short)Length;
	return(Index);
}
