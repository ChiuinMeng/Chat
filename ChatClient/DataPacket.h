#pragma once

enum CMD {
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_NEW_USER_JOIN,
	CMD_ENTER_GROUP_CHAT,
	CMD_ENTER_GROUP_CHAT_RESULT,
	CMD_EXIT_GROUP_CHAT,
	CMD_EXIT_GROUP_CHAT_RESULT,
	CMD_BROADCAST_MESSAGE,
	CMD_BROADCAST_MESSAGE_RESULT,
	CMD_CHAT_WITH_USER,
	CMD_CHAT_WITH_USER_RESULT,
	CMD_USER_MESSAGE,
	CMD_ERROR
};

// 数据报头
struct DataHeader {
	short dataLength; //数据长度
	short cmd;        //命令
};
// 下面均为数据报体
struct Login : public DataHeader {
	Login() {
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	char userName[32];
};
struct LoginResult : public DataHeader {
	LoginResult() {
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RESULT;
	}
	int result;
	int hostID;
};
struct EnterGroupChat : public DataHeader {
	EnterGroupChat() {
		dataLength = sizeof(EnterGroupChat);
		cmd = CMD_ENTER_GROUP_CHAT;
	}
	char userName[32];
};
struct EnterGroupChatResult : public DataHeader {
	EnterGroupChatResult() {
		dataLength = sizeof(EnterGroupChatResult);
		cmd = CMD_ENTER_GROUP_CHAT_RESULT;
		result = 1;
	}
	int result;
};
struct ExitGroupChat : public DataHeader {
	ExitGroupChat() {
		dataLength = sizeof(ExitGroupChat);
		cmd = CMD_EXIT_GROUP_CHAT;
	}
	char userName[32];
};
struct ExitGroupChatResult : public DataHeader {
	ExitGroupChatResult() {
		dataLength = sizeof(ExitGroupChatResult);
		cmd = CMD_EXIT_GROUP_CHAT_RESULT;
		result = 1;
	}
	int result;
};
struct BroadcastMessage :public DataHeader {
	BroadcastMessage() {
		dataLength = sizeof(BroadcastMessage);
		cmd = CMD_BROADCAST_MESSAGE;
		userID = -1;
	}
	char bMessage[256];
	char userName[32];
	int userID;
};
struct BroadcastMessageResult : public DataHeader {
	BroadcastMessageResult() {
		dataLength = sizeof(BroadcastMessageResult);
		cmd = CMD_BROADCAST_MESSAGE_RESULT;
		result = 1;
	}
	int result;
};
struct ChatWithUser : public DataHeader {
	ChatWithUser() {
		dataLength = sizeof(ChatWithUser);
		cmd = CMD_CHAT_WITH_USER;
		receiverID = -1;
	}
	int receiverID;    //欲与之通信的用户的ID，为方便，这里暂时采用socket值。
};
struct ChatWithUserResult : public DataHeader {
	ChatWithUserResult() {
		dataLength = sizeof(ChatWithUserResult);
		cmd = CMD_CHAT_WITH_USER_RESULT;
		receiverStatus = 0;
	}
	int receiverStatus; //欲与之通信的用户的状态，1表示存在该用户。
};
struct UserMessage :public DataHeader {
	UserMessage() {
		dataLength = sizeof(UserMessage);
		cmd = CMD_USER_MESSAGE;
	}
	int receiverID;
	char sendMessage[256];
	char sendUserName[32];
	int senderID;
};