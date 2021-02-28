#include "Qscp.h"
#ifdef WIN32
#include <WinSock2.h>
#include <sys/types.h>
#else
#endif // 
#include "libssh2_config.h"
#include "libssh2.h"
#include "QsshSession.h"
#include <stdio.h>
#include <errno.h>
#include <QThread>
#include <QMutexLocker>
#include <QTextStream>
#include <QDebug>

Qscp::Qscp(QObject *parent)
	: QObject(parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
	p_thread = new QThread;
	QObject::connect(session, &QsshSession::sig_connect, this, &Qscp::slot_connect);
	QObject::connect(session, &QsshSession::sig_scpGet, this, &Qscp::slot_scpGet);
	QObject::connect(session, &QsshSession::sig_scpPut, this, &Qscp::slot_scpPut);
	QObject::connect(session, &QsshSession::sig_list, this, &Qscp::slot_list);
	QObject::connect(session, &QsshSession::sig_changedir, this, &Qscp::slot_changedir);
	QObject::connect(p_thread, &QThread::started, this, &Qscp::slot_connect);
	QObject::connect(p_thread, &QThread::finished, this, &Qscp::slot_disconnect);
	moveToThread(p_thread);
}

Qscp::Qscp(const QString & host, int port, QObject * parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
	p_thread = new QThread;
	session->hostName = host;
	session->hostPort = port;
	QObject::connect(session, &QsshSession::sig_connect, this, &Qscp::slot_connect);
	QObject::connect(session, &QsshSession::sig_scpGet, this, &Qscp::slot_scpGet);
	QObject::connect(session, &QsshSession::sig_scpPut, this, &Qscp::slot_scpPut);
	QObject::connect(session, &QsshSession::sig_list, this, &Qscp::slot_list);
	QObject::connect(session, &QsshSession::sig_changedir, this, &Qscp::slot_changedir);
	QObject::connect(p_thread, &QThread::started, this, &Qscp::slot_connect);
	QObject::connect(p_thread, &QThread::finished, this, &Qscp::slot_disconnect);
	moveToThread(p_thread);
}

Qscp::~Qscp()
{
	if (p_thread) {
		if (p_thread->isRunning()) {
			p_thread->quit();
			p_thread->wait();
		}
		delete p_thread;
		p_thread = NULL;
	}
	delete session;
	session = NULL;
}

void Qscp::setHostName(const QString & host)
{
	if (!session->isConnect())
		session->hostName = host;
}

void Qscp::setPort(int port)
{
	if (!session->isConnect())
		session->hostPort = port;
}

void Qscp::setUserName(const QString & username)
{
	if (!session->isConnect())
		session->userName = username;
}

void Qscp::setPassword(const QString & psd)
{
	if (!session->isConnect())
		session->password = psd;
}

const QString & Qscp::getHostName()
{
	return session->hostName;
}

void Qscp::connectToHost()
{
	if (p_thread->isRunning()) {
		emit session->sig_connect();
	}
	else {
		p_thread->start();
	}
}

void Qscp::disconnect()
{
	session->quitFlag = true;
	p_thread->quit();
	p_thread->wait();
	//session->disconnect();
	session->errStr.clear();
	session->connectState = false;
	currentState = false;
	errStr.clear();
	emit ssh_disconnected();
	return;
}

void Qscp::list(const QString path)
{
	if (session->isConnect())
		emit session->sig_list(path);
}

void Qscp::changedir(const QString & path)
{
	if (session->isConnect())
		emit session->sig_changedir(path);
}

void Qscp::scpGet(const QString & filePath)
{
	if (session->isConnect())
		emit session->sig_scpGet(filePath);
}

void Qscp::scpPut(const QString & filePath, const QString & remotePath)
{
	if (session->isConnect())
		emit session->sig_scpPut(filePath, remotePath);
}

qint64 Qscp::datagramSize()
{
	QMutexLocker locker(&bufferMutex);
	return rcvBuffer.size();
}

qint64 Qscp::readDatagram(char * data, qint64 maxSize)
{
	qint64 readSize = 0;
	bufferMutex.lock();
	if (rcvBuffer.isEmpty())
		return readSize;
	if (rcvBuffer.size() < maxSize) {
		memcpy(data, rcvBuffer.data(), rcvBuffer.size());
		readSize = rcvBuffer.size();
		rcvBuffer.clear();
	}
	else {
		readSize = maxSize;
		memcpy(data, rcvBuffer.data(), maxSize);
		rcvBuffer.remove(0, maxSize);
	}
	return readSize;
}

QByteArray Qscp::readAll()
{
	QMutexLocker locker(&bufferMutex);
	QByteArray datagrame(rcvBuffer);
	rcvBuffer.clear();
	return datagrame;
}

QByteArray Qscp::read(qint64 maxSize)
{
	QMutexLocker locker(&bufferMutex);
	QByteArray datagrame = rcvBuffer.mid(0, maxSize);
	rcvBuffer.remove(0, datagrame.size());
	return datagrame;
}

bool Qscp::initSsh2()
{
	return false;
}

inline bool Qscp::scp_init(const QString & filePath, int openType, LIBSSH2_SFTP_ATTRIBUTES * attrs)
{
	return false;
}

void Qscp::slot_connect()
{
	QMutexLocker locker(&bufferMutex);
	rcvBuffer.clear();
	session->quitFlag = false;

	if (!session->isConnect()) {
		if (!session->connectToHost()) {
			currentState = false;
			errStr = session->errStr;
			emit ssh_error(errStr);
			return;
		}
		else {
			currentState = true;
		}
	}

	if (session->channel) {
		return;
	}
	//create_channel();
	if (currentState)
		emit ssh_connected();
}

void Qscp::slot_disconnect()
{
	//free_channel();

	session->disconnect();
}

void Qscp::slot_scpGet(QString filePath)
{
	int rc = 0;
	int waitcount = 0;
	qint64 got = 0;
	bufferMutex.lock();
	rcvBuffer.clear();
	bufferMutex.unlock();
	if (session->errStr.size()) {
		emit ssh_error(errStr);
		currentState = false;
		return;
	}
	libssh2_struct_stat fileinfo;
	create_channel(1, &fileinfo, NULL, filePath);
	if (session->scpchannel == NULL)
		return;

	char mem[1024 * 512];

	while (got < fileinfo.st_size && !session->quitFlag) {
		int amount = sizeof(mem);

		if ((fileinfo.st_size - got) < amount) {
			amount = (int)(fileinfo.st_size - got);
		}

		rc = libssh2_channel_read(session->scpchannel, mem, amount);
		if (rc > 0) {
			QMutexLocker locker(&bufferMutex);
			//qDebug() << "read from sftp:" << rc;
			rcvBuffer.append(mem, rc);
			locker.unlock();
			got += rc;
			emit ssh_downloadProgress(got, fileinfo.st_size);
			emit ssh_readyRead();
		}
		else if(rc == LIBSSH2_ERROR_EAGAIN)
			waitsocket(session->sock, session->session, 0);
		else{
			char *errmsg = NULL;
			libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
			emit ssh_error(errStr);
			break;
		}
	}

	free_channel(1);
}

void Qscp::slot_scpPut(QString filePath, const QString remotePath)
{
	int rc = 0;
	int waitcount = SSH_CONNECT_WAITCOUNT;
	bufferMutex.lock();
	rcvBuffer.clear();
	bufferMutex.unlock();
	if (session->errStr.size()) {
		emit ssh_error(errStr);
		return;
	}

	struct stat finifo;
	stat(filePath.toUtf8().data(), &finifo);

	//open file
	FILE *tempstorage = fopen(filePath.toUtf8().data(), "rb");
	if (!tempstorage) {
		currentState = false;
		errStr = QString("open file fail:%1").arg(strerror(errno));
		emit ssh_error(errStr);
		return;
	}

	create_channel(0, NULL, &finifo, remotePath);
	if (session->scpchannel == NULL)
		return;

	qint64 currentByte = 0;
	fseek(tempstorage, 0L, SEEK_END);
	qint64 totalByte = ftell(tempstorage);
	fseek(tempstorage, 0L, SEEK_SET);
	char mem[1024 * 512] = { 0 };
	size_t nread = 0;
	char *ptr = NULL;

	do {
		nread = fread(mem, 1, sizeof(mem), tempstorage);
		if (nread <= 0) {
			/* end of file */
			break;
		}
		ptr = mem;

		currentByte += nread;
		do {
			waitcount = SSH_CONNECT_WAITCOUNT;
			while ((rc = libssh2_channel_write(session->scpchannel, ptr, nread)) ==
				LIBSSH2_ERROR_EAGAIN && waitcount) {
				if (waitsocket(session->sock, session->session, 1) > 0)
					continue;
				waitcount--;
			}
			if (rc < 0 && !waitcount) {
				currentState = false;
				errStr = QString("SCP upload timed out: %1\n").arg(rc);
				break;
			}
			else {
				/* rc indicates how many bytes were written this time */
				nread -= rc;
				ptr += rc;
			}
		} while (nread && !session->quitFlag);
		emit ssh_uploadProgress(currentByte, totalByte);
	} while (!session->quitFlag && currentState); /* only continue if nread was drained */


	fclose(tempstorage);

	if (!currentState)
		emit ssh_error(errStr);

	free_channel(0);
}

void Qscp::slot_list(QString path)
{
	errStr.clear();
	LOCAL_DIR_LIST dirlist;

	QString pwdpath = path;
	if (!pwdpath.startsWith("cd"))
		pwdpath.insert(0, "cd ");
	pwdpath.append(";pwd");

	path.insert(0, SSH_CMD_LIST);

	QByteArray resMsg, errMsg;
	if (!exec_cmd(pwdpath, resMsg, errMsg))
		return;
	if (resMsg.size()) {
		dirlist.loacalPath = QString::fromUtf8(resMsg).simplified();
		resMsg.clear();
		errMsg.clear();
		if (!exec_cmd(path, resMsg, errMsg))
			return;
		if (resMsg.size()) {
			QTextStream textstream(resMsg);
			textstream.setCodec("utf-8");
			qDebug() << textstream.readLine();
			//analyze the file info
			while (!textstream.atEnd() && !session->quitFlag)
			{
				LOCAL_FILE_INFO fileinfo;
				QStringList strlist = textstream.readLine().simplified().split(" ");
				if (strlist.size() < 8)
					continue;
				fileinfo.permissions = strlist.at(0);
				if (fileinfo.permissions.at(0) != 'd') {
					fileinfo.filetypes = 1;
				}
				fileinfo.owner = strlist.at(2);
				fileinfo.filesize = strlist.at(4).toLongLong();
				fileinfo.date = QString().append(strlist.at(5)).append(" ").append(strlist.at(6));
				fileinfo.names = strlist.at(7);
				dirlist.files.append(fileinfo);
			}
		}
		else {
			errStr = QString::fromUtf8(errMsg);
			emit ssh_error(errStr);
			return;
		}
	}
	else {
		errStr = QString::fromUtf8(errMsg);
		emit ssh_error(errStr);
		return;
	}
	emit ssh_dirChanged(dirlist);
}

void Qscp::slot_changedir(QString path)
{
	slot_list(path);
}

bool Qscp::exec_cmd(const QString & cmd, QByteArray & resMsg, QByteArray & errMsg)
{
	create_cmdChannel();
	if (!session->channel)
		return false;

	errStr.clear();
	int rc = 0, rc1 = 0;
	int waitcount = SSH_CONNECT_WAITCOUNT;

	while ((rc = libssh2_channel_exec(session->channel, cmd.toUtf8().data())) ==
		LIBSSH2_ERROR_EAGAIN && waitcount) {
		waitsocket(session->sock, session->session, 0);
		waitcount--;
	}
	/*while ((rc = libssh2_channel_write (session->channel, cmd.toUtf8().data(),1)) ==
	LIBSSH2_ERROR_EAGAIN && waitcount) {
	waitsocket(session->sock, session->session, 0);
	waitcount--;
	}*/

	if (rc != 0) {
		if (waitcount == 0)
			errStr = "execute time out";
		else {
			char *errmsg = NULL;
			rc = libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
		}
		emit ssh_error(errStr);
		return false;
	}
	//int bytecount = 0;
	while (!session->quitFlag) {
		/* loop until we block */
		do {
			rc = libssh2_channel_read(session->channel, buffer, sizeof(buffer));
			rc1 = libssh2_channel_read_stderr(session->channel, buffer1, sizeof(buffer1));
			if (rc > 0) {
				resMsg.append(buffer, rc);
			}
			else {
				//if (rc != LIBSSH2_ERROR_EAGAIN);
				/* no need to output this for the EAGAIN case */
				//fprintf(stderr, "libssh2_channel_read returned %d\n", rc);
			}

			if (rc1 > 0) {
				errMsg.append(buffer, rc);
			}
			else {
				//if (rc1 != LIBSSH2_ERROR_EAGAIN);
				/* no need to output this for the EAGAIN case */
				//fprintf(stderr, "libssh2_channel_read_error returned %d\n", rc1);
			}
		} while ((rc > 0 || rc1 > 0) && !session->quitFlag);

		/* this is due to blocking that would occur otherwise so we loop on
		this condition */
		if (rc == LIBSSH2_ERROR_EAGAIN || rc1 == LIBSSH2_ERROR_EAGAIN) {
			waitsocket(session->sock, session->session, 0);
		}
		else
			break;
	}
	free_cmdChannel();
	return true;
}

inline void Qscp::create_cmdChannel()
{
	int waitcount = SSH_CONNECT_WAITCOUNT;
	while ((session->channel = libssh2_channel_open_session(session->session)) == NULL &&
		libssh2_session_last_error(session->session, NULL, NULL, 0) ==
		LIBSSH2_ERROR_EAGAIN && waitcount) {
		waitsocket(session->sock, session->session, 0);
		waitcount--;
	}

	if (!session->channel) {
		currentState = false;
		if (waitcount == 0)
			errStr = "create ssh channel fail: request time out";
		else {
			char *errmsg = NULL;
			libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
		}
		emit ssh_error(errStr);
		return;
	}
}

inline void Qscp::free_cmdChannel()
{
	if (session->channel) {
        char *exitsignal = (char*)"none";
		int rc = 0;
		int exitcode = 0;
		while ((rc = libssh2_channel_close(session->channel)) == LIBSSH2_ERROR_EAGAIN)
			waitsocket(session->sock, session->session, 0);

		if (rc == 0) {
			exitcode = libssh2_channel_get_exit_status(session->channel);
			libssh2_channel_get_exit_signal(session->channel, &exitsignal,
				NULL, NULL, NULL, NULL, NULL);
		}

		//if (exitsignal)
		//	fprintf(stderr, "\nGot signal: %s\n", exitsignal);
		//else
		//	fprintf(stderr, "\nEXIT: %d bytecount: %d\n", exitcode, bytecount);

		libssh2_channel_free(session->channel);
		session->channel = NULL;
	}
}

inline void Qscp::create_channel(int scpType, libssh2_struct_stat *fileinfo, struct stat *finfo, const QString &scppath)
{
	//0 is send type 1 is receive type
	switch (scpType)
	{
	case 0:
	{
		do 
		{
			session->scpchannel = libssh2_scp_send(session->session, scppath.toUtf8().data()
				, finfo->st_mode & 0777, (unsigned long)finfo->st_size);
			if ((!session->scpchannel) && (libssh2_session_last_errno(session->session) !=
				LIBSSH2_ERROR_EAGAIN)) {
				char *err_msg;
				libssh2_session_last_error(session->session, &err_msg, NULL, 0);
				errStr = QString::fromUtf8(err_msg);
				break;
			}
		} while (!session->scpchannel && !session->quitFlag);
	}
		break;
	case 1:
	{
		/* Request a file via SCP */
		do 
		{
			session->scpchannel = libssh2_scp_recv2(session->session, scppath.toUtf8().data(), fileinfo);
			if ((!session->scpchannel) && (libssh2_session_last_errno(session->session) !=
				LIBSSH2_ERROR_EAGAIN)) {
				char *err_msg;
				libssh2_session_last_error(session->session, &err_msg, NULL, 0);
				errStr = QString::fromUtf8(err_msg);
				break;
			}
		} while (!session->scpchannel && !session->quitFlag);
	}
		break;
	default:
		break;
	}
	if (session->scpchannel == NULL) {
		emit ssh_error(errStr);
	}
}

inline void Qscp::free_channel(int scpType)
{
	switch (scpType)
	{
	case 0:
	{
		while (libssh2_channel_send_eof(session->scpchannel) == LIBSSH2_ERROR_EAGAIN);
		while (libssh2_channel_wait_eof(session->scpchannel) == LIBSSH2_ERROR_EAGAIN);
		while (libssh2_channel_wait_closed(session->scpchannel) == LIBSSH2_ERROR_EAGAIN);
	}
		break;
	case 1:
	default:
		break;
	}
	libssh2_channel_free(session->scpchannel);
	session->scpchannel = NULL;
}
