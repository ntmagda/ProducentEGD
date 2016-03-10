#include <windows.h>
#include <stdio.h>
#include <conio.h>
char gefHelpText[]= {
"\nReflectEGD is a console mode sample that accepts EGD messages from anyone and\n"
"resends them to TCP/IP address or computer name specified as an optional 1st\n"
"parameter. If 2nd parameter is added, it is a TCP/IP address to use as Producer\n"
"ID when resending(use any letter to resend using host TCP/IP address).\n"
"Examples are:\n"
"  ReflectEGD       ;list EGD data received from other devices without resending\n"
"  ReflectEGD 3.16.45.10  ; resend EGD data to that address keeping ProducerID\n"
"  ReflectEGD 3.16.45.10  3.16.45.20  ;resend EGD with new ProducerID 2nd param.\n"
"  ReflectEGD mycomputer  host ; 1st param. computer name, use host as Producer\n"
"  ReflectEGD echo         ;echo received EGD back to any sender, for StressEGD \n\n"
"This program has been developed by Doug MacLeod at GE Fanuc PLC Tech Support as\n"
"the simplest possible EGD program to do something useful. To create project\n"
"with MS VisualC 5/6, create a new project as a win32 console app and change the\n"
"project/settings/link Object/library modules list to include wsock32.lib\n\n"
"For a complete application, look at CommEGD EGD server code on our web site at\n"
"www.gefanuc.com/support/plc site under Logicmaster 90 product in the Files area\n"
"Use this program with the StressEGD program for network EGD performance testing\n"};
#pragma pack(2)		// change default packing from 4 or 8 bytes to 2
#define GEF_EGD_UDP_DATA_PORT  0x4746	/* Letters GF used as port for EGD Data message*/
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
GEF_EGD_DATA MessageEGD;
int main(int argc, char *argv[])
{

    unsigned char dataToSend [1400];
    memset(dataToSend, 1, 1400);

    MessageEGD.PDUTypeVersion   = 0x010d; /* */
    MessageEGD.RequestID        = 1;
    MessageEGD.ProducerID       = inet_addr("127.0.0.1");
    MessageEGD.ExchangeID       = 1;
    MessageEGD.TimeStampSec     = time(NULL);
    MessageEGD.TimeStampNanoSec = 0;
    MessageEGD.Status           = 1;
    MessageEGD.ConfigSignature  = 0;
    MessageEGD.Reserved         = 0;


	char Text[_MAX_PATH],*pText;
	HOSTENT *pHostEntry;
	SOCKET HostSocket,TargetSocket;
	struct in_addr HostAddress,TargetAddress,FromAddress;
	struct sockaddr_in HostSocketAddress,TargetSocketAddress,FromSocketAddress;
	int Letter,Loop,ReadyForEGD,EchoBackToSender;
    long ByteLength,DataLength,FromLength,TargetID,NewProducerID;

    memcpy(MessageEGD.ProductionData, dataToSend, 1400);
    ByteLength = DataLength + 32;

#ifdef WIN32
    WSADATA wsaData;
//	Setup Windows socket requesting v1.1 WinSock
    if (WSAStartup(MAKEWORD(1,1), &wsaData)) {
		printf("\nTCP/IP network does not appear to be installed");
		return(0);
	}
	SetConsoleTitle("ReflectEGD");
#endif
//	Get short and full host computer name and TCP/IP address
	gethostname(Text,sizeof(Text));
	pHostEntry = gethostbyname(Text);
	ReadyForEGD = TRUE;
	printf("Tu jestem!\n");
	if (pHostEntry) {
		memcpy(&HostAddress,pHostEntry->h_addr_list[0],sizeof(DWORD));
		pText = (char *)inet_ntoa(HostAddress);
		printf("\nv1.0 dated Jul-00, Press ? key for help info or Q to Quit\nComputer is %s/(or %s) = %s",Text,pHostEntry->h_name,pText);
//	Create datagram socket and bind to port "GF" to receive EGD messages
	    HostSocket = socket(AF_INET, SOCK_DGRAM,0);
		TargetSocket = socket(AF_INET, SOCK_DGRAM,0);
		if ((HostSocket == INVALID_SOCKET)||(TargetSocket == INVALID_SOCKET)) {
			printf("\nError %d, Not able to create sockets. Check if TCP/IP networking installed",WSAGetLastError());
			ReadyForEGD = FALSE;
		}
		else {
			memset(&HostSocketAddress,0,sizeof(HostSocketAddress));
			HostSocketAddress.sin_family = AF_INET;
			HostSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
			HostSocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); //ustawienie adresu IP
			if (bind(HostSocket, (LPSOCKADDR)&HostSocketAddress, sizeof(HostSocketAddress)) == SOCKET_ERROR) {
				printf("\nError %d, Not able to bind this computer socket to EGD data port GF",WSAGetLastError());
				ReadyForEGD = FALSE;
			}
		}
	}
	else {
		printf("\nDid not find TCP/IP address for computer %s",Text);
		ReadyForEGD = FALSE;
	}
	printf("\nTu jestem i Ready for EGD = %d!\n",ReadyForEGD);
	EchoBackToSender = FALSE;
	TargetID = 0;
	NewProducerID = 0;
//	If parameters on command line, get target address(1st) and new Producter ID (2nd)
	if (ReadyForEGD&&(argc>1)) {
	    printf("\nWszed³em\n");
		pText = argv[1];
		printf("%s\n",pText);
		TargetID = inet_addr(pText);
		if (TargetID==INADDR_NONE) {
			if (stricmp(pText,"echo")) {
				pHostEntry = gethostbyname(pText);
				if (pHostEntry) {
					memcpy(&TargetAddress,pHostEntry->h_addr_list[0],sizeof(DWORD));
					memcpy(&TargetID,&TargetAddress,sizeof(DWORD));
					pText = (char *)inet_ntoa(TargetAddress);
				}
				else {
					pText = "Error, 1st parameter not recognized";
					TargetID = 0;
				}
			}
			else {
				pText = "echo back to sender";
				EchoBackToSender = TRUE;
			}
		}
		printf("\nReflect all EGD messages to %s",pText);
		memcpy(&TargetSocketAddress,&HostSocketAddress,sizeof(HostSocketAddress));
		memcpy(&TargetSocketAddress.sin_addr,&TargetID,sizeof(long));
		if (argc>2) {
			pText = argv[2];
			NewProducerID = inet_addr(pText);
			if (NewProducerID==INADDR_NONE) {
				memcpy(&NewProducerID,&HostAddress,sizeof(long));
				pText = (char *)inet_ntoa(HostAddress);
			}
			printf(" changing Producer ID to %s",pText);
		}
	}




	while (ReadyForEGD) {
//	Check if any UDP datagrams on this port. If none, check for key, then give up this tick
		ioctlsocket (HostSocket, FIONREAD, &ByteLength);
		if (ByteLength<=0) {
			if(kbhit()) {
				Letter = getch();
				if ((Letter=='Q')||(Letter=='q')) {
					break;
				}
				if (Letter=='?') {
					printf("%s",gefHelpText);
				}
				else {
					printf("\nEnter Q to quit or any other key to continue");
					Letter = getch();
				}
			}
//	Sleep for 1 MS may give up rest of tick, a 0 waits less, but locks out lower priority tasks
			Sleep(1);
		}
		else {
                printf("jestesmy w else\n");
//	Received UDP datagram on this port
			FromLength = sizeof(FromSocketAddress);
		    ByteLength = recvfrom(HostSocket,(char *)&MessageEGD, sizeof(MessageEGD), 0, (LPSOCKADDR)&FromSocketAddress, &FromLength);
		    if (ByteLength == SOCKET_ERROR) {
				printf("Error %d receiving UDP datagram",WSAGetLastError());
		    }
			else {
				DataLength = ByteLength - 32;
//	Check data length and Type and version for EGD data
				if ((DataLength>0)&&(MessageEGD.PDUTypeVersion==0x010D)) {
					memcpy(&FromAddress,&MessageEGD.ProducerID,sizeof(DWORD));
//					if (!EchoBackToSender) {
						printf("\nReceived %u data bytes from %s Exchange %u, Request %u",
							DataLength,(char *)inet_ntoa(FromAddress),MessageEGD.ExchangeID,MessageEGD.RequestID);
//					}
//	Resend EGD message if 1st parameter address, changing ProducerId if 2nd parameter. If echo, reply
					if (TargetID || EchoBackToSender) {
						if (NewProducerID ) {
							MessageEGD.ProducerID = NewProducerID;
						}
						if (EchoBackToSender) {
							memcpy(&TargetSocketAddress.sin_addr,&FromAddress,sizeof(long));
						}
						sendto(TargetSocket,(char *)&MessageEGD,ByteLength,0,(LPSOCKADDR)&TargetSocketAddress,sizeof(TargetSocketAddress));
					}
					else {
//	If not resending or echoing, dump up to first 24 bytes as hex data
						if (DataLength>24) DataLength = 24;
						printf("\nData=");
						for (Loop=0; Loop<DataLength; Loop++) {
							printf("%2X ",MessageEGD.ProductionData[Loop]);
						}
					}
				}
				else {
					printf("\nNon EGD datagram received containing %u bytes",ByteLength);
				}
			}
		}
	}
    if (HostSocket) closesocket(HostSocket);
    if (TargetSocket) closesocket(TargetSocket);
#ifdef WIN32
	WSACleanup();
#endif
	return(0);
}
