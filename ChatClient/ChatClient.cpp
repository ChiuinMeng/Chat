// Linux环境编译命令：
//     g++ -o ChatClient ChatClient.cpp -pthread

#ifdef _WIN32
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
	#include <winsock2.h>
	#include <Windows.h>
	#include <inaddr.h>
	#pragma comment(lib, "ws2_32")
#else
	#include<unistd.h> //uni std
	#include<arpa/inet.h>
	typedef unsigned int SOCKET;
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif
#include <iostream>
#include <thread>
#include <cstring>

#include "DataPacket.h"

#include "DataPacket.h"


enum STATUS {
	STATUS_GROUP_CHAT,
	STATUS_PERSONAL_CAHT,
	STATUS_DEFAULT
};

bool g_bRun = true;            //全局变量，表示客户端运行状态，false时应退出。
char g_userName[32] = {};      //用户名
int  g_myID = -1;               //本机ID 
int  g_receiverID = -1;         //与之通信的id
int  g_status = STATUS_DEFAULT; //客户端状态

int receive_print_message(SOCKET _cSock);
int cmdThread(SOCKET _sock);

int main(int argc, char* argv[])
{
	char ip[16] = "127.0.0.1";
	if (argc == 2) {
		strcpy(ip, argv[1]);  //将命令参数设为IP地址
		printf("从命令行获取到IP参数，ip = %s\n", ip);
	}
	// 启动Windows socket 2.x环境
#ifdef _WIN32
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0)  return -1;
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		printf("version failed");
		return -1;
	}
	else {
		printf("version success");
	}
#endif

	// 创建socket套接字
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("erro, create socket failed\n");
	}
	else {
		printf("create socket success\n");
	}

	unsigned int uiRcvBuf;
	int uiRcvBufLen = sizeof(uiRcvBuf);
	if (-1 != getsockopt(_sock, SOL_SOCKET, SO_SNDBUF, (char*)& uiRcvBuf, &uiRcvBufLen)) {
		printf("获取系统默认的发送数据缓冲区大小, uiRcvBuf = %d\n", uiRcvBuf);
	}
	else {
		printf("获取系统默认的发送数据缓冲区大小失败\n");
	}


	// 连接socket
	sockaddr_in name = { 0 };
	name.sin_family = AF_INET;
	name.sin_port = htons(6600);
#ifdef _WIN32
	// name.sin_addr.S_un.S_addr = htonl(**);
	name.sin_addr.S_un.S_addr = inet_addr(ip);
#else
	name.sin_addr.s_addr = inet_addr(ip);
#endif
	if (SOCKET_ERROR == connect(_sock, (sockaddr*)& name, sizeof(sockaddr_in))) {
		printf("connect socket failed\n");
		g_bRun = false;
	}
	else {
		printf("connect socket success\n");
	}

	// 启动线程函数
	std::thread t1(cmdThread, _sock);
	t1.detach();         //线程分离


	fd_set fdRead;   //描述符(socket)集合
	fd_set fdWrite;
	fd_set fdExp;
	while (g_bRun) {
		//清理集合
		FD_ZERO(&fdRead);
		FD_ZERO(&fdWrite);
		FD_ZERO(&fdExp);
		//将描述符（socket）加入集合
		FD_SET(_sock, &fdRead);
		FD_SET(_sock, &fdWrite);
		FD_SET(_sock, &fdExp);
		timeval t = { 0,0 };
		int s_r = select(_sock, &fdRead, &fdWrite, &fdExp, &t);
		if ( s_r < 0) {
			printf("select failed\n");
			break;
		}
		else if (s_r == 0) {
			// 指定socket集合无事件发生。
		}
		else {
			if (FD_ISSET(_sock, &fdRead)) {  //若该socket可读，接受数据
				FD_CLR(_sock, &fdRead);
				if (-1 == receive_print_message(_sock)) {
					printf("processor failed\n");
					break;
				}
			}
			if (FD_ISSET(_sock, &fdWrite)) { //若该socket可写，发送数据
				FD_CLR(_sock, &fdWrite);
				// TODO
			}
			if (FD_ISSET(_sock, &fdExp)) {   //若该socket异常
				FD_CLR(_sock, &fdExp);
				// TODO
			}
		}
		// TODO: 空闲时间处理其它业务
	}


	// 收尾工作
#ifdef _WIN32
	closesocket(_sock);
	WSACleanup();
#else
	close(_sock);
#endif
	getchar();
	return 0;
}

/*
	接受socket中的消息 并 打印出来。
	不要在这里send数据！
*/
int  receive_print_message(SOCKET _cSock)
{
	char szRecv[4096] = {};  //缓冲区

	// 1. 接受服务器数据
	int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0); //先接受数据头
	if (nLen <= 0) {
		printf("连接服务器失败\n");
		return -1;
	}
	DataHeader* header = (DataHeader*)szRecv;
	recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);//再接受数据体

	// 2. 根据数据头中的命令，选择不同分支，处理不同请求。
	switch (header->cmd) {
	case CMD_LOGIN_RESULT: {
		LoginResult* login = (LoginResult*)szRecv;
		printf("本机ID为：%d\n", login->hostID);
		g_myID = login->hostID;
		break;
	}
	case CMD_ENTER_GROUP_CHAT_RESULT: {
		EnterGroupChatResult* egcr = (EnterGroupChatResult*)szRecv;
		if (egcr->result == 1) {
			g_status = STATUS_GROUP_CHAT;    //客户端进入群聊状态
			printf("已进入群聊室:\n");
		}
		break;
	}
	case CMD_EXIT_GROUP_CHAT_RESULT: {
		ExitGroupChatResult* egcr = (ExitGroupChatResult*)szRecv;
		if (egcr->result == 1) {
			g_status = STATUS_DEFAULT;       //客户端退出群聊状态
			printf("已退出群聊.\n");
		}
		break;
	}
	case CMD_BROADCAST_MESSAGE: {
		BroadcastMessage* bm = (BroadcastMessage*)szRecv;
		if (g_status == STATUS_GROUP_CHAT)
			printf("<%d|%s> %s\n", bm->userID, bm->userName, bm->bMessage); //收到其它用户的群聊消息
		break;
	}

	case CMD_BROADCAST_MESSAGE_RESULT: {
		BroadcastMessageResult* bmr = (BroadcastMessageResult*)szRecv;
		if (bmr->result != 1) printf("发送群聊消息失败\n");
		break;
	}
	case CMD_CHAT_WITH_USER_RESULT: {
		ChatWithUserResult* cwur = (ChatWithUserResult*)szRecv;
		if (cwur->receiverStatus == 1) {      //若该用户存在
			g_status = STATUS_PERSONAL_CAHT;
			printf("<%d|%s> : ", g_myID, g_userName);
		}
		else {
			printf("该用户不在线\n");
		}
		break;
	}
	case CMD_USER_MESSAGE: {
		UserMessage* um = (UserMessage*)szRecv;
		if (g_status == STATUS_PERSONAL_CAHT && g_receiverID == um->senderID) {
			printf("<%d|%s> : %s\n", um->senderID, um->sendUserName, um->sendMessage);
		}
		else {
			printf("你有一条来自<%s:%d>的私人消息：%s\n", um->sendUserName, um->senderID, um->sendMessage);
		}
		break;
	}
	default:
		printf("收到未知命令;%d, 长度:%d\n", header->cmd, header->dataLength);
		break;
	}
	return 0;
};

/*
   命令线程，用于接受用户输入的命令 并 发送相应的数据到服务器
   不要在这里调用recv函数！
*/
int cmdThread(SOCKET _sock) {
	while (g_bRun) {
		char cmdBuf[256] = {};           //暂存用户输入命令
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit")) {
			printf("exit\n");
			g_bRun = false;
			return -1;
		}
		else if (0 == strcmp(cmdBuf, "help")) {
			printf("你可以使用如下命令:\n");
			printf("    exit                 退出\n");
			printf("    register <username>  以用户名注册，返回本机ID\n");
			printf("    egc                  enter group chat 进入群聊\n");
			printf("    eegc                 exit  group chat 退出群聊 \n");
			printf("    cw <userID>          chat with user 与指定用户聊天\n");
			printf("    ecw                  end chat with user 终止与某用户的聊天\n");
		}
		else if (0 == strcmp(cmdBuf, "register")) {
			Login login;
			scanf("%s", login.userName);
			strcpy(g_userName, login.userName);
			send(_sock, (const char*)& login, sizeof(login), 0);
		}
		else if (0 == strcmp(cmdBuf, "egc")) {
			EnterGroupChat egc;
			strcpy(egc.userName, g_userName);
			send(_sock, (const char*)& egc, sizeof(egc), 0);
		}
		else if (0 == strcmp(cmdBuf, "eegc")) {
			ExitGroupChat egc;
			strcpy(egc.userName, g_userName);
			send(_sock, (const char*)& egc, sizeof(egc), 0);
		}
		else if (0 == strcmp(cmdBuf, "cw")) {
			scanf("%d", &g_receiverID);
			ChatWithUser cwu;
			cwu.receiverID = g_receiverID;
			send(_sock, (const char*)& cwu, sizeof(cwu), 0);
		}
		else if (0 == strcmp(cmdBuf, "ecw")) {
			g_status = STATUS_DEFAULT;
			printf("已结束私人聊天.\n");
		}
		else {
			if (g_status == STATUS_GROUP_CHAT) {
				BroadcastMessage bm;
				strcpy(bm.bMessage, cmdBuf);
				strcpy(bm.userName, g_userName);
				bm.userID = g_myID;
				send(_sock, (const char*)& bm, sizeof(bm), 0);
			}
			else if (g_status == STATUS_PERSONAL_CAHT) {
				UserMessage um;
				strcpy(um.sendMessage, cmdBuf);
				strcpy(um.sendUserName, g_userName);
				um.receiverID = g_receiverID;
				um.senderID = g_myID;
				send(_sock, (const char*)& um, sizeof(um), 0);
			}
			else {
				printf("未知命令，请输入help查看可用命令\n");
			}
		}
	}
	return 0;
}

/*
	封装send函数，若出现错误，打印错误信息
*/
int my_send(SOCKET s, const char FAR* buf, int len, int flags) {
	int send_result = send(s, buf, len, flags);
	if (send_result == SOCKET_ERROR) {    // 如果发现错误，显示错误,方便调试
		int lastError = WSAGetLastError();
		printf("发送数据失败，错误码%d :", lastError);
		switch (lastError)
		{
		case WSAEWOULDBLOCK: {
			printf("WSAEWOULDBLOCK");  //该错误码说明发送缓冲区满了
			break;
		}
		default:
			break;
		}
		printf("  \n");
	}
	return send_result;
}

/* WIKI
FD_WRITE事件触发条件：
	1. client 通过connect（WSAConnect）首次和server建立连接时，在client端会触发FD_WRITE事件。
	2. server 通过accept（WSAAccept）接受client连接请求时，在server端会触发FD_WRITE事件。
	3. send（WSASend）/sendto（WSASendTo）发送失败返回WSAEWOULDBLOCK，并且当缓冲区有可用空间时，则会触发FD_WRITE事件。
注：
	1和2其实是同一种情况，在第一次建立连接时，C/S端都会触发一个FD_WRITE事件。
	send出去的数据其实都先存在winsock的发送缓冲区中，然后才发送出去，如果缓冲区满了，那么
再调用send（WSASend,sendto,WSASendTo）的话，就会返回一个 WSAEWOULDBLOCK的错误码，接下来随
着发送缓冲区中的数据被发送出去，缓冲区中出现可用空间时，一个 FD_WRITE 事件才会被触发，这里
比较容易混淆的是 FD_WRITE 触发的前提是 “缓冲区要先被充满然后随着数据的发送又出现可用空间”
，而不是缓冲区中有可用空间。

	因此，作为一个应用程序，自收到首条FD_WRITE消息开始，便应认为自己必然能在一个套接字上发
出数据，直至一个send、WSASend、sendto或WSASendTo返回套接字错误WSAEWOULDBLOCK。经过了这样的
失败以后，要再用另一条FD_WRITE通知应用程序再次发送数据。
*/