#include <windows.h>
#include <stdio.h>
#include <conio.h>

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


unsigned char* targetIP;
unsigned char* hostIP;
GEF_EGD_DATA MessageEGD;
SOCKET HostSocket,TargetSocket;


long prepareMessage(unsigned char *dataToSend) {   // return ByteLength

    long ByteLength;
    MessageEGD.PDUTypeVersion   = 0x010d; /* */
    MessageEGD.RequestID        = 1;
    MessageEGD.ProducerID       = inet_addr(targetIP);
    MessageEGD.ExchangeID       = 1;
    MessageEGD.TimeStampSec     = time(NULL);
    MessageEGD.TimeStampNanoSec = 0;
    MessageEGD.Status           = 1;
    MessageEGD.ConfigSignature  = 0;
    MessageEGD.Reserved         = 0;
    memcpy(MessageEGD.ProductionData, dataToSend, 1400);
    ByteLength = 1400+32;

    return ByteLength;
}


/* IP of your PLC*/
void setTargetIP (unsigned char* IP){
    targetIP = IP;
}
/* IP of your PC*/
void setHostIP (unsigned char* IP){
    hostIP = IP;
}

void closeConnection() {
    shutdown(TargetSocket,2);
    shutdown(HostSocket,2);
}


int setConnection(struct sockaddr_in HostSocketAddress) {
    return bind(HostSocket, (LPSOCKADDR)  &HostSocketAddress, sizeof(HostSocketAddress));
}

void sendMessage(long ByteLength, struct sockaddr_in TargetSocketAddress ) {
    sendto(TargetSocket,(char *)&MessageEGD,ByteLength,0,(LPSOCKADDR)&TargetSocketAddress,sizeof(TargetSocketAddress));
}
