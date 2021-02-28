#pragma once
#include "qssh2_global.h"
#include "ssh_global.h"
#include <functional>
#include <QObject>
#include <QMutex>

class QsshSession;
class QThread;

class QSSH2_EXPORT Qssh : public QObject
{
	Q_OBJECT

public:
	Qssh(QObject *parent = NULL);
	Qssh(const QString &host, int port = 22, QObject *parent = NULL);
	~Qssh();

	void setHostName(const QString &host);
	void setPort(int port = 22);
	void setUserName(const QString &username);
	void setPassword(const QString &psd);

	const QString &getHostName();

	bool connectToHost();
	void disconnect();

	void exec(const QString &cmd);

	bool connectState() { return currentState; }

	qint64 readData(QByteArray &rcvArray, const QByteArray &strend = "$", int timeout = 1000);

	const QString &getLastError() { return errStr; }
private:
	Qssh(const Qssh&) = delete;
	Qssh &operator=(const Qssh &) = delete;

	//session
	QsshSession *session = NULL;
	//LIBSSH2_SFTP *sftp_session = NULL;
	//LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;
	char buffer[1024 * 256];

	bool currentState = false;
	QString errStr;
	std::function<int(int, LIBSSH2_SESSION *, qint64)> waitsocket = NULL;
signals:
	//void ssh_error(const QString&);
	//void ssh_readyRead();
	//void ssh_finished();
	//void ssh_disconnected();
	//void ssh_connected();
private:
	void slot_connect();
	void slot_disconnect();
	bool slot_exec(QString cmd);

	inline void create_channel();
	inline void free_channel();
};
