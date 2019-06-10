#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <Windows.h>
#include <inaddr.h>
#pragma comment(lib, "ws2_32")

#include <iostream>
#include <vector>
#include <cstring>

#include "DataPacket.h"

std::vector<SOCKET> g_clients;

int  processor(SOCKET _cSock);

int main()
{
	// 1.请求版本号
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

	// 2.创建一个socket
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);


	// 3.创建协议地址族
	SOCKADDR_IN addrSrv = { 0 };
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//IP地址
	addrSrv.sin_port = htons(6600);//端口号
	if (SOCKET_ERROR == bind(serverSocket, (SOCKADDR*)& addrSrv, sizeof(SOCKADDR))) {
		printf("erro, bind port failed\n");
	}
	else {
		printf("bind port success\n");
	}


	// 5.监听
	if (SOCKET_ERROR == listen(serverSocket, 10)) { //监听数：10
		printf("listen port failed\n");
	}
	else {
		printf("listen port success\n");
	}

	// 6.通信
	while (true)   //循环处理socket消息
	{
		//伯克利 BSD socket
		fd_set fdRead;  //描述符(socket)集合
		fd_set fdWrite;
		fd_set fdExp;
		//清理集合
		FD_ZERO(&fdRead);
		FD_ZERO(&fdWrite);
		FD_ZERO(&fdExp);
		//将描述符（socket）加入集合
		FD_SET(serverSocket, &fdRead);
		FD_SET(serverSocket, &fdWrite);
		FD_SET(serverSocket, &fdExp);
		for (unsigned int n = 0; n < g_clients.size(); n++) {
			FD_SET(g_clients[n], &fdRead);
		}

		//select第一个参数nfds是一个整数值，是指fd_set集合中的所有描述符（socket）的范围，而不是大小。
		//既是所有文件描述符的最大值+1，在Windows中这个参数可以为0（因为宏的存在）
		//timeout参数若为NULL，select会阻塞!
		//timeout设置为0，则不会阻塞，直接返回。
		//timeout表示的是最大的一个时间值， 实际上并不一定会阻塞那么久。
		timeval t = { 0,0 };
		if (select(serverSocket + 1, &fdRead, &fdWrite, &fdExp, &t) < 0) {
			printf("select finish\n");
			break;
		}
		if (FD_ISSET(serverSocket, &fdRead)) {
			FD_CLR(serverSocket, &fdRead);
			SOCKADDR_IN addr = { 0 };  //存客户端的网络地址
			int addrlen = sizeof(SOCKADDR_IN);
			SOCKET clientSocket = INVALID_SOCKET;
			clientSocket = accept(serverSocket, (sockaddr*)& addr, &addrlen);//因为select了，所以不会在这里阻塞
			if (INVALID_SOCKET == clientSocket) {
				printf("accept failed\n");
			}
			else {
				g_clients.push_back(clientSocket);     //将客户端socket添加进动态数组
				printf("accept success, new client: socket = %d, ip = %s\n", clientSocket, inet_ntoa(addr.sin_addr));
			}
		}
		for (unsigned int n = 0; n < fdRead.fd_count; n++) {
			if (-1 == processor(fdRead.fd_array[n])) {  //处理客户端请求
				auto iter = find(g_clients.begin(), g_clients.end(), fdRead.fd_array[n]);
				if (iter != g_clients.end()) {
					g_clients.erase(iter);
				}
			}
		}
		// TODO: 服务器做其它事情，即便没有客户端事件发生。
		// 空闲时间，处理其它业务
	}

	// 收尾工作
	for (int n = g_clients.size() - 1; n >= 0; n--) {
		closesocket(g_clients[n]);
	}
	closesocket(serverSocket);
	WSACleanup();
	getchar(); //防止程序一闪而过
	return	0;
}

/*
	处理客户端消息
*/
int  processor(SOCKET _cSock)
{
	char szRecv[4096] = {};// 缓冲区

	// 接受客户端数据
	int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0); //先接受数据头DataHeader
	if (nLen <= 0) {
		printf("client<%d> exit\n", _cSock);
		return -1; //该客户端已断开连接
	}

	DataHeader* header = (DataHeader*)szRecv;
	recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0); //再收取后半段数据体

	// 根据数据头中的命令，选择不同分支，处理不同请求。
	switch (header->cmd) {
	case CMD_LOGIN: {
		Login* login = (Login*)szRecv;
		printf("receive command from client<%d>: CMD_LOGIN, data length: %d, userName=%s\n", _cSock, login->dataLength, login->userName);
		//忽略判断用户密码是否正确的过程
		LoginResult ret;
		ret.hostID = _cSock;
		send(_cSock, (char*)& ret, sizeof(LoginResult), 0);
		break;
	}
	case CMD_ENTER_GROUP_CHAT: {
		printf("client<%d>:CMD_ENTER_GROUP_CHAT ，dLen: %d\n", _cSock, header->dataLength);
		EnterGroupChat* egc = (EnterGroupChat*)szRecv;
		printf("%s 加入群聊\n", egc->userName);

		BroadcastMessage bm;
		char bMessage[256] = {};
		strcat(bMessage, egc->userName);
		strcat(bMessage, "加入群聊");
		strcpy(bm.bMessage, bMessage);
		strcpy(bm.userName, egc->userName);
		bm.userID = _cSock;
		for (unsigned int i = 0; i < g_clients.size(); i++) {
			if (g_clients[i] == _cSock)continue;
			send(g_clients[i], (char*)& bm, sizeof(BroadcastMessage), 0);//?
		}

		EnterGroupChatResult egcr;
		egcr.result = 1;
		send(_cSock, (char*)& egcr, sizeof(egcr), 0);
		break;
	}
	case CMD_EXIT_GROUP_CHAT: {
		printf("client<%d>:CMD_EXIT_GROUP_CHAT ，dLen: %d\n", _cSock, header->dataLength);
		ExitGroupChat* egc = (ExitGroupChat*)szRecv;
		printf("%s 退出群聊\n", egc->userName);

		BroadcastMessage bm;
		char bMessage[256] = {};
		strcat(bMessage, egc->userName);
		strcat(bMessage, "退出群聊");
		strcpy(bm.bMessage, bMessage);
		strcpy(bm.userName, egc->userName);
		bm.userID = _cSock;
		for (unsigned int i = 0; i < g_clients.size(); i++) {
			if (g_clients[i] == _cSock)continue;
			send(g_clients[i], (char*)& bm, sizeof(BroadcastMessage), 0);
		}

		ExitGroupChatResult egcr;
		send(_cSock, (char*)& egcr, sizeof(egcr), 0);
		break;
	}
	case CMD_BROADCAST_MESSAGE: {
		printf("client<%d>:CMD_BROADCAST_MESSAGE ，dLen: %d\n", _cSock, header->dataLength);
		BroadcastMessage* bm = (BroadcastMessage*)szRecv;
		printf("用户:%s 发送群聊消息:%s", bm->userName, bm->bMessage);
		bm->userID = _cSock;

		for (unsigned int i = 0; i < g_clients.size(); i++) {
			if (g_clients[i] == _cSock)continue;
			send(g_clients[i], szRecv, sizeof(BroadcastMessage), 0);
		}

		BroadcastMessageResult bmr;
		send(_cSock, (char*)& bmr, sizeof(bmr), 0);
		break;
	}
	case CMD_CHAT_WITH_USER: {
		printf("client<%d>:CMD_CHAT_WITH_USER ，dLen: %d\n", _cSock, header->dataLength);
		ChatWithUser* cwu = (ChatWithUser*)szRecv;

		unsigned int i = 0;
		for (; i < g_clients.size(); i++) {
			if (g_clients[i] == _cSock)continue;
			if (g_clients[i] == cwu->receiverID)break;
		}

		ChatWithUserResult cwur;
		if (i == g_clients.size()) cwur.receiverStatus = 0;
		else cwur.receiverStatus = 1;
		send(_cSock, (char*)& cwur, sizeof(cwur), 0);
		break;
	}
	case CMD_USER_MESSAGE: {
		printf("client<%d>:CMD_USER_MESSAGE ，dLen: %d\n", _cSock, header->dataLength);
		UserMessage* um = (UserMessage*)szRecv;
		um->senderID = _cSock;

		unsigned int i = 0;
		for (; i < g_clients.size(); i++) {
			if (g_clients[i] == um->receiverID) {
				send(g_clients[i], szRecv, sizeof(UserMessage), 0);
				break;
			}
		}
		if (i == g_clients.size()) {
			ChatWithUserResult cwur;
			cwur.receiverStatus = 0;
			send(_cSock, (char*)& cwur, sizeof(cwur), 0);
		}
		break;
	}
	default:
		printf("client<%d>: 未知命令, dLen: %d\n", _cSock, header->dataLength);
		DataHeader h;
		h.cmd = CMD_ERROR;
		h.dataLength = sizeof(DataHeader);
		send(_cSock, (char*)& h, sizeof(h), 0);
		break;
	}
	return 0;
}