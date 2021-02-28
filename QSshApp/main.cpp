#include <WinSock2.h>
#include <QtWidgets/QApplication>
#include "RemoteExplore.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

	WSADATA wsadata;
	int err;

	err = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if (err != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", err);
		return 1;
	}

    //QSshApp w;
    //w.show();
	RemoteExplore re;
	re.show();

	int res = a.exec();

	WSACleanup();
    return res;
}
