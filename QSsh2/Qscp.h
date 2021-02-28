#pragma once
#include "qssh2_global.h"
#include "ssh_global.h"
#include <functional>
#include <QObject>
#include <QMutex>

class QsshSession;
class QThread;

class QSSH2_EXPORT Qscp : public QObject
{
	Q_OBJECT

public:
	Qscp(QObject *parent = NULL);
	Qscp(const QString &host, int port = 22, QObject *parent = NULL);
	~Qscp();

	void setHostName(const QString &host);
	void setPort(int port = 22);
	void setUserName(const QString &username);
	void setPassword(const QString &psd);

	const QString &getHostName();

	void connectToHost();
	void disconnect();
	void list(const QString path = QString());
	void changedir(const QString &path);
	void scpGet(const QString &filePath);
	void scpPut(const QString &filePath, const QString &remotePath);

	qint64 datagramSize();
	qint64 readDatagram(char *data, qint64 maxSize);
	QByteArray readAll();
	QByteArray read(qint64 maxSize);

	bool connectState() { return currentState; }
private:
	Qscp(const Qscp&) = delete;
	Qscp &operator=(const Qscp &) = delete;
	//session
	QsshSession *session = NULL;
	char buffer[1024 * 128];
	char buffer1[1024 * 128];
	//buffer
	QByteArray rcvBuffer;
	QMutex bufferMutex;
	//thread
	QThread *p_thread = NULL;

	bool currentState = false;
	QString errStr;

	bool initSsh2();

	inline bool scp_init(const QString &filePath, int openType = 0, LIBSSH2_SFTP_ATTRIBUTES *attrs = NULL);

	std::function<int(int, LIBSSH2_SESSION *, qint64)> waitsocket = NULL;

	//int waitsocket(int socket_fd, LIBSSH2_SESSION *session,qint64 sec = 0);
signals:
	void ssh_error(const QString&);
	void ssh_readyRead();
	void ssh_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void ssh_uploadProgress(qint64 bytesSent, qint64 bytesTotal);
	void ssh_finished();
	void ssh_disconnected();
	void ssh_connected();
	void ssh_dirChanged(LOCAL_DIR_LIST);
private:
	void slot_connect();
	void slot_disconnect();
	void slot_scpGet(QString);
	void slot_scpPut(QString, QString);

	void slot_list(QString path = QString());
	void slot_changedir(QString path);

	bool exec_cmd(const QString &cmd, QByteArray &resMsg, QByteArray &errMsg);

	inline void create_cmdChannel();
	inline void free_cmdChannel();
	inline void create_channel(int scpType, libssh2_struct_stat *fileinfo, struct stat *finfo, const QString &scppath);
	inline void free_channel(int scpType);
};
