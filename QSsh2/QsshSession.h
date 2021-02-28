#pragma once

#include "qssh2_global.h"
#include <QObject>
#include "ssh_global.h"

class QsshSession : public QObject
{
	Q_OBJECT
	friend class Qsftp;
	friend class Qssh;
	friend class Qscp;
public:
	~QsshSession();
private:
	enum Qssh_Type
	{
		TYPE_SSH,
		TYPE_SFTP,
		TYPE_SCP
	};
	QsshSession(QObject *parent = NULL);
	bool connectToHost();
	bool disconnect();
	bool isConnect();
	bool hasLastErr();
	const QString &getErrStr();

	static int waitsocket(int socket_fd, LIBSSH2_SESSION *session, qint64 sec = 0);
private:
	QsshSession(const QsshSession &) = delete;
	QsshSession &operator=(const QsshSession&) = delete;

	QString hostName;
	int hostPort = 22;
	QString userName;
	QString password;
	//socket
	int sock = 0;

	bool connectState = false;
	bool quitFlag = false;
	QString errStr;

	LIBSSH2_SESSION *session = NULL;
	LIBSSH2_CHANNEL *channel = NULL;
	LIBSSH2_CHANNEL *scpchannel = NULL;
	LIBSSH2_KNOWNHOSTS *nh = NULL;

	//sftp
	LIBSSH2_SFTP *sftp_session = NULL;
	LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;
signals:
	void sig_connect();
	void sig_disconnect();
	void sig_sftpGet(QString);
	void sig_sftpPut(QString,QString);
	void sig_scpGet(QString);
	void sig_scpPut(QString, QString);
	void sig_exec(QString);
	void sig_test();
	void sig_list(QString path = QString());
	void sig_changedir(QString);
};
