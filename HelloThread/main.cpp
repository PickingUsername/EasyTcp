#include<iostream>
#include<thread>
#include<mutex>
#include<atomic>//ԭ��
using namespace std;

mutex m;
atomic_int sum = 0;

void workFun(int index) {
	for (size_t i = 0; i < 8000000; i++)
	{
		//�Խ��� һ����������Զ�unlock
		//lock_guard<mutex> lg(m);
		sum++;
	}
		
}

int main() {

	thread t[3];
	for (int i = 0; i < 3; i++) {
		t[i] = thread(workFun,i);
	}
	for (int i = 0; i < 3; i++) {
		t[i].join();
	}

	cout << sum << endl;
	getchar();
	return 0;

}