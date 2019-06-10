#ifdef _WIN32  //Windows操作系统
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include<WINSOCK2.H>
#pragma comment(lib,"WS2_32.lib")
#else	//非Windows操作系统
#include<unistd.h> //uni std
#include<arpa/inet.h> 
typedef unsigned int SOCKET;
#endif

#include<string.h>
#include<iostream>

using namespace std;

int main()
{
#ifdef _WIN32
	WSADATA wsaData;//初始化
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	SOCKET RecvSocket;
	sockaddr_in RecvAddr;//服务器地址
	int Port = 4000;//服务器监听地址
	char RecvBuf[1024] = {};//发送数据的缓冲区
	int BufLen = 1024;//缓冲区大小
	sockaddr_in SenderAddr;
#ifdef _WIN32
	int SenderAddrSize = sizeof(sockaddr_in);
#else
	unsigned int SenderAddrSize = sizeof(SenderAddr);
#endif


	//创建接收数据报的socket
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//将socket与制定端口和0.0.0.0绑定
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (sockaddr*)& RecvAddr, sizeof(RecvAddr));

	while (true) {
		memset(RecvBuf, 0, sizeof(RecvBuf));
		recvfrom(RecvSocket, RecvBuf, BufLen, 0, (sockaddr*)& SenderAddr, &SenderAddrSize);
		printf("收到消息：%s\n", RecvBuf);

		memset(RecvBuf, 0, sizeof(RecvBuf));
		strcpy(RecvBuf, "你好，客户端，收到你的消息了");
		sendto(RecvSocket, RecvBuf, strlen(RecvBuf) + 1, 0, (sockaddr*)& SenderAddr, sizeof(sockaddr));
	}

	//关闭socket，结束接收数据
	printf("finished receiving,closing socket..\n");
#ifdef _WIN32 
	closesocket(RecvSocket);
	WSACleanup();
#else
	close(RecvSocket);
#endif 
	printf("Exiting.\n");
	getchar();
	return 0;
}

/*
	编译命令
		g++ **.cpp             //linux
		g++ **.cpp -l ws2_32   //windows
*/