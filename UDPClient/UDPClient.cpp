#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include<WINSOCK2.H>
#pragma comment(lib,"WS2_32.lib")  // g++编译得使用g++ UDPServer.cpp -l WS2_32
#else
#include<unistd.h> //uni std
#include<arpa/inet.h> 
typedef unsigned int SOCKET;
#endif // _WIN32

#include<iostream>
#include<string.h>
#include <thread>

#include "DataPacket.h"

using namespace std;

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

int  processor(const char* data_buf);
int cmdThread(SOCKET _sock, const sockaddr* to, int tolen);

int main(int argc, char* argv[])
{
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	int Port = 10086;               //服务器监听地址
	sockaddr_in RecvAddr;          //服务器地址
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	char cp[] = "127.0.0.1";       //服务器IP地址
	if (argc == 2) strcpy(cp, argv[1]);
	RecvAddr.sin_addr.s_addr = inet_addr(cp);

	//创建Socket对象
	SOCKET SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//bind(SendSocket, (sockaddr*)& RecvAddr, sizeof(RecvAddr));

	// 启动线程函数
	std::thread t1(cmdThread, SendSocket, (sockaddr*)& RecvAddr, sizeof(RecvAddr));
	t1.detach();         //线程分离

	char data_buf[2056] = {};//发送数据的缓冲区
	while (g_bRun) {
		memset(data_buf, 0, sizeof(data_buf));
		int fromlen = sizeof(sockaddr);
		if (-1 == recvfrom(SendSocket, data_buf, sizeof(data_buf), 0, (sockaddr*)& RecvAddr, &fromlen)) {
			continue; //未接受到数据
		}
		else {
			printf("接受到一条消息\n");
			if (-1 == processor(data_buf)) break;
		}
	}

	printf("程序结束，关闭socket\n");
#ifdef _WIN32
	closesocket(SendSocket);
	WSACleanup();
#else
	close(SendSocket);
#endif
	printf("按任意键结束...\n");
	getchar();
	return 0;
}

/*
   命令线程，用于接受用户输入的命令
   不要在这里调用recv函数。
*/
int cmdThread(SOCKET _sock, const sockaddr* to, int tolen) {
	while (g_bRun) {
		char cmdBuf[256] = {};           //暂存用户输入命令
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit")) {
			printf("exit\n");
			g_bRun = false;
			exit(-1);
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

			sendto(_sock, (const char*)& login, sizeof(login), 0, to, tolen);
		}
		else if (0 == strcmp(cmdBuf, "egc")) {
			EnterGroupChat egc;
			strcpy(egc.userName, g_userName);
			sendto(_sock, (const char*)& egc, sizeof(egc), 0, to, tolen);
		}
		else if (0 == strcmp(cmdBuf, "eegc")) {
			ExitGroupChat egc;
			strcpy(egc.userName, g_userName);
			sendto(_sock, (const char*)& egc, sizeof(egc), 0, to, tolen);
		}
		else if (0 == strcmp(cmdBuf, "cw")) {
			scanf("%d", &g_receiverID);
			ChatWithUser cwu;
			cwu.receiverID = g_receiverID;
			sendto(_sock, (const char*)& cwu, sizeof(cwu), 0, to, tolen);
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
				sendto(_sock, (const char*)& bm, sizeof(bm), 0, to, tolen);
			}
			else if (g_status == STATUS_PERSONAL_CAHT) {
				UserMessage um;
				strcpy(um.sendMessage, cmdBuf);
				strcpy(um.sendUserName, g_userName);
				um.receiverID = g_receiverID;
				um.senderID = g_myID;
				sendto(_sock, (const char*)& um, sizeof(um), 0, to, tolen);
			}
			else {
				printf("未知命令，请输入help查看可用命令\n");
			}
		}
	}
	return 0;
}

/*
	处理接受到的消息
*/
int  processor(const char* data_buf)
{
	DataHeader* header = (DataHeader*)data_buf;

	// 根据数据头中的命令，选择不同分支，处理不同请求。
	switch (header->cmd) {
	case CMD_LOGIN_RESULT: {
		LoginResult* login = (LoginResult*)data_buf;
		printf("本机ID为：%d\n", login->hostID);
		g_myID = login->hostID;
		break;
	}
	case CMD_ENTER_GROUP_CHAT_RESULT: {
		EnterGroupChatResult* egcr = (EnterGroupChatResult*)data_buf;
		if (egcr->result == 1) {
			g_status = STATUS_GROUP_CHAT;    //客户端进入群聊状态
			printf("已进入群聊室:\n");
		}
		break;
	}
	case CMD_EXIT_GROUP_CHAT_RESULT: {
		ExitGroupChatResult* egcr = (ExitGroupChatResult*)data_buf;
		if (egcr->result == 1) {
			g_status = STATUS_DEFAULT;       //客户端退出群聊状态
			printf("已退出群聊.\n");
		}
		break;
	}
	case CMD_BROADCAST_MESSAGE: {
		BroadcastMessage* bm = (BroadcastMessage*)data_buf;
		if (g_status == STATUS_GROUP_CHAT)
			printf("<%d|%s> %s\n", bm->userID, bm->userName, bm->bMessage); //收到其它用户的群聊消息
		break;
	}

	case CMD_BROADCAST_MESSAGE_RESULT: {
		BroadcastMessageResult* bmr = (BroadcastMessageResult*)data_buf;
		if (bmr->result != 1) printf("发送群聊消息失败\n");
		break;
	}
	case CMD_CHAT_WITH_USER_RESULT: {
		ChatWithUserResult* cwur = (ChatWithUserResult*)data_buf;
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
		UserMessage* um = (UserMessage*)data_buf;
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