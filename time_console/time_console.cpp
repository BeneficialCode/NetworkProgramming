// time_console.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <stdio.h>
#include <time.h>


int main() {
	time_t timer;
	time(&timer);

	printf("Local time is: %s", ctime(&timer));
	return 0;
}

