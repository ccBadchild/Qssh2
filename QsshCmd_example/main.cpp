#include <QtCore/QCoreApplication>
#include <WinSock2.h>
#include <stdio.h>
#include <iostream>
#include "Qssh.h"
#include <QDebug>

using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
#ifdef WIN32
	system("chcp 65001");
#endif
	WSADATA wsadata;
	int err;

	err = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if (err != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", err);
		return 1;
	}
	Qssh ssh;
	ssh.setHostName("127.0.0.1");
	ssh.setUserName("root");
	ssh.setPassword("psd");
	if (!ssh.connectToHost())
	{
		qDebug() << ssh.getLastError();
		exit(1);
	}

	QByteArray readArray;
	ssh.readData(readArray, "#");
	printf("%s", readArray.data());
	readArray.clear();
	string getin;
	while (1) {
		getline(cin, getin);
		ssh.exec(getin.c_str());
		//system("cls");
		ssh.readData(readArray);
		if (readArray.isEmpty()) {
			qDebug() << ssh.getLastError();
		}
		else {
			printf("%s", readArray.data());
			fflush(stdout);
		}
		if (getin == "exit") {
			printf("enter any key to exit");
			fflush(stdout);
			ssh.disconnect();
			getchar();
			WSACleanup();
			exit(0);
		}
		getin.clear();
		readArray.clear();
	}
	WSACleanup();
    return a.exec();
}
