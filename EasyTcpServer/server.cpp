#include "EasyTcpServer.hpp"
#include<thread>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}
class MyServer:public EasyTcpServer
{
public:
	MyServer() {
	}
	~MyServer() {
	
	}
	virtual void OnJoin(ClientSocket* pClient) 
	{
		_clientCount++;
		printf("client<%d> join\n", pClient->sockfd());
	}

	virtual void OnLeave(ClientSocket* pClient)
	{
		_clientCount--;
		printf("client<%d> leava\n", pClient->sockfd());
	}

	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{

			Login* login = (Login*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			LoginResult ret;
			pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			LogoutResult ret;
			pClient->SendData(&ret);
		}
		break;
		default:
		{
			printf("收到未定义消息,数据长度：%d\n", header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		break;
		}
	}

private:

};

int main()
{
	MyServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(50);
	server.Start(4);
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun)
	{
		server.OnRun();
		//printf("空闲时间处理其它业务..\n");
	}
	server.Close();
	printf("已退出。\n");
	getchar();
	return 0;
}