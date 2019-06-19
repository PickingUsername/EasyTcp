#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
#define FD_SETSIZE      2506
#define WIN32_LEAN_AND_MEAN
//#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<windows.h>
#include<WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include<unistd.h> //uni std
#include<arpa/inet.h>
#include<string.h>

#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

#include<stdio.h>
#include<vector>
#include<thread>
#include<mutex>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"
#include<atomic>
//缓冲区最小单元大小
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 10240
#endif // !RECV_BUFF_SZIE



//客户端数据类型
class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getLastPos()
	{
		return _lastPos;
	}
	void setLastPos(int pos)
	{
		_lastPos = pos;
	}

	//发送指定Socket数据
	int SendData( DataHeader* header)
	{
		if (header)
		{
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

private:
	// socket fd_set  file desc set
	SOCKET _sockfd;
	//第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SZIE * 5];
	//消息缓冲区的数据尾部位置
	int _lastPos;
};

//网络事件接口
class INetEvent
{
public:
	//客户端离开事件
	virtual  void OnJoin(ClientSocket* pClient) = 0;//纯虚函数
		//客户端离开事件
	virtual  void OnLeave(ClientSocket* pClient) = 0;//纯虚函数
	//客户端消息事件
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) = 0;
private:

};

class CellServer
{
public:
	CellServer(SOCKET sock = INVALID_SOCKET) {
		_sock = sock;
		_pThread = nullptr;
		_pNetEvent = nullptr;
	}
	~CellServer() {
		Close();
		delete _pThread;
		_sock = INVALID_SOCKET;
	}

	void setEventObj(INetEvent* event) 
	{
		_pNetEvent = event;
	}

	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//关闭Socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8 关闭套节字closesocket
			closesocket(_sock);
#else
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8 关闭套节字closesocket
			close(_sock);
#endif
			_clients.clear();
		}
	}

	//处理网络消息
//int _nCount = 0;
	bool OnRun()
	{
		while (isRun())
		{
			if (_clientsBuff.size() > 0)
			{
				//从缓冲队列里取出客户数据
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff)
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//如果没有需要处理的客户端，就跳过
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//伯克利套接字 BSD socket
			fd_set fdRead;//描述符（socket） 集合
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符（socket）加入集合
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}
			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);

			if (ret < 0)
			{
				printf("select任务结束。\n");
				Close();
				return false;
			}

			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(_clients[n]))
					{
						auto iter = _clients.begin() + n;//std::vector<SOCKET>::iterator
						if (iter != _clients.end())
						{
							_pNetEvent->OnLeave(_clients[n]);
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
	}

	//缓冲区
	char _szRecv[RECV_BUFF_SZIE] = {};

	//接收数据 处理粘包 拆分包
	int RecvData(ClientSocket* pClient)
	{
		
		// 5 接收客户端数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SZIE, 0);
		//printf("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			//printf("客户端<Socket=%d>已退出，任务结束。\n", pClient->sockfd());
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nLen);

		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = pClient->getLastPos() - header->dataLength;
				//处理网络消息
				OnNetMsg(pClient, header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			else {
				//消息缓冲区剩余数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_pNetEvent->OnNetMsg(pClient, header);
		
	}

	//连接缓冲区内的客户端
	void addClient(ClientSocket* pClient)
	{
		//_mutex.lock();
		std::lock_guard<std::mutex> lock(_mutex);
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}

	void Start()
	{
		_pThread = new std::thread(std::mem_fn(&CellServer::OnRun), this);
		_pThread->detach();
	}

	size_t getClientCount() 
	{
		return _clients.size() + _clientsBuff.size();
	}

	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			SendData(_clients[n]->sockfd(), header);
		}
	}

	int SendData(SOCKET cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

private:
	SOCKET _sock;
	//正式客户队列
	std::vector<ClientSocket*> _clients;
	//缓冲客户队列
	std::vector<ClientSocket*> _clientsBuff;
	std::mutex _mutex;
	std::thread* _pThread;
	INetEvent* _pNetEvent;
};

class EasyTcpServer : public INetEvent
{
private:
	SOCKET _sock;
	//消息处理对象，内部会创建线程
	std::vector<CellServer*> _cellServers;
	CELLTimestamp _tTime;

protected:
	//收到消息计数
	std::atomic_int _recvCount;
	//客户端加入计数
	std::atomic_int _clientCount;

public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}
	//初始化Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("错误，建立socket失败...\n");
		}
		else {
			printf("建立socket=<%d>成功...\n", (int)_sock);
		}
		return _sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		//if (INVALID_SOCKET == _sock)
		//{
		//	InitSocket();
		//}
		// 2 bind 绑定用于接受客户端连接的网络端口
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip) {
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip) {
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("错误,绑定网络端口<%d>失败...\n", port);
		}
		else {
			printf("绑定网络端口<%d>成功...\n", port);
		}
		return ret;
	}

	//监听端口号
	int Listen(int n)
	{
		// 3 listen 监听网络端口
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("socket=<%d>错误,监听网络端口失败...\n",(int)_sock);
		}
		else {
			printf("socket=<%d>监听网络端口成功...\n", (int)_sock);
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == cSock)
		{
			printf("socket=<%d>错误,接受到无效客户端SOCKET...\n", (int)_sock);
		}
		else
		{
			//将客户端分配给最合适的cellServer
			addClientToCellServer(new ClientSocket(cSock));
			//获取IP inet_ntoa(clientAddr.sin_addr);
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		//查找客户数量最少的CellServer消息处理对象
		auto pMinCellServer = _cellServers[0];
		for (auto pCellServer:_cellServers)
		{
			if (pMinCellServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinCellServer = pCellServer;
			}
		}
		pMinCellServer->addClient(pClient);
		OnJoin(pClient);
	}

	//启动消息处理服务器
	void Start(int nCellServer)
	{
		for (int i = 0; i < nCellServer; i++)
		{
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			//注册网络事件接受对象
			ser->setEventObj(this);
			//启动消息处理线程
			ser->Start();
		}
	}

	//关闭Socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			// 8 关闭套节字closesocket
			closesocket(_sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			// 8 关闭套节字closesocket
			close(_sock);
#endif
		}
	}

	//处理网络消息
	//int _nCount = 0;
	bool OnRun()
	{
		timeForMsg();
		if (isRun())
		{
			//伯克利套接字 BSD socket
			fd_set fdRead;//描述符（socket） 集合
			//fd_set fdWrite;
			//fd_set fdExp;
			//清理集合
			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);
			//将描述符（socket）加入集合
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);
			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			timeval t = { 0,10};
			int ret = select(_sock + 1, &fdRead, 0, 0, &t); //
			//printf("select ret=%d count=%d\n", ret, _nCount++);
			if (ret < 0)
			{
				printf("select任务结束。\n");
				Close();
				return false;
			}
			//判断描述符（socket）是否在集合中
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//缓冲区
	char _szRecv[RECV_BUFF_SZIE] = {};

	//响应网络消息
	void timeForMsg()
	{
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			int clientNum = _clientCount;
			printf("thread<%d>,time<%lf>,socket<%d>,clients<%d>,recvCount<%d>\n", (int)_cellServers.size(),t1, (int)_sock, clientNum, (int)(_recvCount / t1));
			_recvCount = 0;
			_tTime.update();
		}
		
	}

	//发送指定Socket数据
	int SendData(SOCKET cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	void SendDataToAll(DataHeader* header)
	{
		for (auto cellServer : _cellServers) {
			cellServer->SendDataToAll(header);
		}
	}

	virtual void OnJoin(ClientSocket* pClient) {
		_clientCount++;
	}

	virtual void OnLeave(ClientSocket* pClient) {
		_clientCount--;
	}

	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
	}

};

#endif // !_EasyTcpServer_hpp_
