
#include "wysylanieEGD.h"
#pragma pack(2)		// change default packing from 4 or 8 bytes to 2

int main()
{
    int i;
    unsigned char dataToSend [1400];
    long ByteLength;
    memset(dataToSend, 1, 1400);
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
    #endif

    setTargetIP("127.0.0.1");
    setHostIP("127.0.0.1");


    printf(targetIP);
    printf(hostIP);

    ByteLength = prepareMessage(dataToSend);


    HostSocket = socket(AF_INET, SOCK_DGRAM,0);
    TargetSocket = socket(AF_INET, SOCK_DGRAM,0);


    if ((HostSocket == INVALID_SOCKET)||(TargetSocket == INVALID_SOCKET)) {
        printf("INVALID SOCKETS");
    }

    memset(&HostSocketAddress,0,sizeof(HostSocketAddress));
    HostSocketAddress.sin_family = AF_INET;
    HostSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
    HostSocketAddress.sin_addr.s_addr = inet_addr(targetIP);

    if (setConnection(HostSocketAddress) == SOCKET_ERROR) {
				printf("\nError %d, Not able to bind this computer socket to EGD data port GF", WSAGetLastError());
				ReadyToSendEGD = FALSE;
    }
    else {
        ReadyToSendEGD = TRUE;
    }

    memset(&TargetSocketAddress,0,sizeof(TargetSocketAddress));
    TargetSocketAddress.sin_family = AF_INET;
    TargetSocketAddress.sin_port = htons(GEF_EGD_UDP_DATA_PORT);
    TargetSocketAddress.sin_addr.s_addr = inet_addr(hostIP);

    if(ReadyToSendEGD) {
        sendMessage(ByteLength, TargetSocketAddress);
    }

    closeConnection();
    return 0;

}
