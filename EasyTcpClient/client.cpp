#include "EasyTcpClient.hpp"
#include<thread>
#include<mutex>
using namespace std;

//�ͻ�������
const int cCount = 8;
//�����߳�����
const int tCount = 4;
//�ͻ�������
EasyTcpClient* client[cCount];
//��
mutex m;

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
			printf("�˳�cmdThread�߳�\n");
			break;
		}
		else {
			printf("��֧�ֵ����\n");
		}
	}
}

void sendThread(int id) //1~4
{
	printf("thread<%d>,enter\n", id);
	int c = (cCount / tCount);
	int begin = (id - 1)*c;
	int end = id * c;
	for (int n = begin; n < end; n++)
	{

		client[n] = new EasyTcpClient();
	}
	for (int n = begin; n < end; n++)
	{
		client[n]->Connect("127.0.0.1", 4567);
	}
	printf("thread<%d>,Connect<begin=%d,end=%d>\n", id, begin,end);

	std::chrono::milliseconds t(3000);
	std::this_thread::sleep_for(t);

	Login login;
	strcpy(login.userName, "lyd");
	strcpy(login.PassWord, "lydmm");
	while (g_bRun)
	{
		for (int n = begin; n < end; n++)
		{
			client[n]->SendData(&login);
			client[n]->OnRun();
		}
	}

	for (int n = begin; n < end; n++)
	{
		client[n]->Close();
		delete client[n];
	}
	printf("thread<%d>,exit\n", id);
}

int main()
{
	//����UI�߳�
	std::thread t1(cmdThread);
	t1.detach();

	//���������߳�
	for (int i = 0; i < tCount; i++)
	{
		std::thread t1(sendThread,i+1);
		t1.detach();
	}

	while (g_bRun)
		Sleep(100);
	printf("���˳���\n");
	return 0;
}