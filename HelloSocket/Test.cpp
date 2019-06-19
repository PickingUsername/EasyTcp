#define WIIN32_LEAN_AND_MEAN

#include<WinSock2.h>
#include<Windows.h>

//#pragma comment(lib,"ws2_32.lib")

int main() {
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
	//




	//
	WSACleanup();
	return 0;
}