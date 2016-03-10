#include <windows.h>
#include <stdio.h>
#include <conio.h>

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

long prepareMessage(GEF_EGD_DATA *MessageEGD, unsigned char *dataToSend) {   // return ByteLength

    long ByteLength;
    MessageEGD->PDUTypeVersion   = 0x010d; /* */
    MessageEGD->RequestID        = 1;
    MessageEGD->ProducerID       = inet_addr("127.0.0.1");
    MessageEGD->ExchangeID       = 1;
    MessageEGD->TimeStampSec     = time(NULL);
    MessageEGD->TimeStampNanoSec = 0;
    MessageEGD->Status           = 1;
    MessageEGD->ConfigSignature  = 0;
    MessageEGD->Reserved         = 0;
    memcpy(MessageEGD->ProductionData, dataToSend, 1400);
    ByteLength = 1400+32;

    return ByteLength;
}

int main()
{
    GEF_EGD_DATA MessageEGD;
    unsigned char dataToSend [1400];
    long ByteLength;
    memset(dataToSend, 1, 1400);
    SOCKET HostSocket,TargetSocket;
    struct in_addr HostAddress,TargetAddress,FromAddress;
	struct sockaddr_in HostSocketAddress,TargetSocketAddress;
	int ReadyToSendEGD = FALSE;
	#ifdef WIN32
    WSADATA wsaData;
//	Setup Windows socket requesting v1.1 WinSock
    if (WSAStartup(MAKEWORD(1,1), &wsaData)) {
		printf("\nTCP/IP network does not appear to be installed");
		return(0);
	}
	SetConsoleTitle("ReflectEGD");
    #endif
    ByteLength = prepareMessage(&MessageEGD, dataToSend);


    HostSocket = socket(AF_INET, SOCK_DGRAM,0);
    TargetSocket = socket(AF_INET, SOCK_DGRAM,0);


    if ((HostSocket == INVALID_SOCKET)||(TargetSocket == INVALID_SOCKET)) {
        printf("INVALID SOCKETS");
    }

    memset(&HostSocketAddress,0,sizeof(HostSocketAddress));
    HostSocketAddress.sin_family = AF_INET;
    HostSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
    HostSocketAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(HostSocket, (LPSOCKADDR)  &HostSocketAddress, sizeof(HostSocketAddress)) == SOCKET_ERROR) {
				printf("\nError %d, Not able to bind this computer socket to EGD data port GF", WSAGetLastError());
				ReadyToSendEGD = FALSE;
    }

    memset(&TargetSocketAddress,0,sizeof(TargetSocketAddress));
    TargetSocketAddress.sin_family = AF_INET;
    TargetSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
    TargetSocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(ReadyToSendEGD) {
        sendto(TargetSocket,(char *)&MessageEGD,ByteLength,0,(LPSOCKADDR)&TargetSocketAddress,sizeof(TargetSocketAddress));
    }

    return 0;

}



