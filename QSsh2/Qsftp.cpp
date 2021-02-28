#include "Qsftp.h"
#ifdef WIN32
#include <WinSock2.h>
#include <sys/types.h>
#else
#endif // 
#include "libssh2_config.h"
#include "libssh2.h"
#include "libssh2_sftp.h"
#include "QsshSession.h"
#include <stdio.h>
#include <errno.h>
#include <QThread>
#include <QMutexLocker>
#include <QTextStream>
#include <QDebug>

#pragma execution_character_set("utf-8")

Qsftp::Qsftp(QObject * parent)
	:QObject(parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
	p_thread = new QThread;
	QObject::connect(session, &QsshSession::sig_connect, this, &Qsftp::slot_connect);
	QObject::connect(session, &QsshSession::sig_sftpGet, this, &Qsftp::slot_sftpGet);
	QObject::connect(session, &QsshSession::sig_sftpPut, this, &Qsftp::slot_sftpPut);
	QObject::connect(session, &QsshSession::sig_list, this, &Qsftp::slot_list);
	QObject::connect(session, &QsshSession::sig_changedir, this, &Qsftp::slot_changedir);
	QObject::connect(p_thread, &QThread::started, this, &Qsftp::slot_connect);
	QObject::connect(p_thread, &QThread::finished, this, &Qsftp::slot_disconnect);
	moveToThread(p_thread);
}

Qsftp::Qsftp(const QString & host, int port, QObject * parent)
	: QObject(parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
	p_thread = new QThread;
	session->hostName = host;
	session->hostPort = port;
	QObject::connect(session, &QsshSession::sig_connect, this, &Qsftp::slot_connect);
	QObject::connect(session, &QsshSession::sig_sftpGet, this, &Qsftp::slot_sftpGet);
	QObject::connect(session, &QsshSession::sig_sftpPut, this, &Qsftp::slot_sftpPut);
	QObject::connect(session, &QsshSession::sig_list, this, &Qsftp::slot_list);
	QObject::connect(session, &QsshSession::sig_changedir, this, &Qsftp::slot_changedir);
	QObject::connect(p_thread, &QThread::started, this, &Qsftp::slot_connect);
	QObject::connect(p_thread, &QThread::finished, this, &Qsftp::slot_disconnect);
	moveToThread(p_thread);
}

Qsftp::~Qsftp()
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

void Qsftp::setHostName(const QString & host)
{
	if(!session->isConnect())
		session->hostName = host;
}

void Qsftp::setPort(int port)
{
	if (!session->isConnect())
		session->hostPort = port;
}

void Qsftp::setUserName(const QString & username)
{
	if (!session->isConnect())
		session->userName = username;
}

void Qsftp::setPassword(const QString & psd)
{
	if (!session->isConnect())
		session->password = psd;
}

const QString & Qsftp::getHostName()
{
	return session->hostName;
}

void Qsftp::connectToHost()
{
	//emit session->sig_connect();
	if (p_thread->isRunning()) {
		emit session->sig_connect();
	}
	else {
		p_thread->start();
	}
}

void Qsftp::disconnect()
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

void Qsftp::list(const QString path)
{
	if (session->isConnect())
		emit session->sig_list(path);
}

void Qsftp::changedir(const QString & path)
{
	if (session->isConnect())
		emit session->sig_changedir(path);
}

void Qsftp::sftpGet(const QString & filePath)
{
	if(session->isConnect())
		emit session->sig_sftpGet(filePath);
}

void Qsftp::sftpPut(const QString & filePath, const QString &remotePath)
{
	if (session->isConnect())
		emit session->sig_sftpPut(filePath, remotePath);
}

qint64 Qsftp::datagramSize()
{
	QMutexLocker locker(&bufferMutex);
	return rcvBuffer.size();
}

qint64 Qsftp::readDatagram(char * data, qint64 maxSize)
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

QByteArray Qsftp::readAll()
{
	QMutexLocker locker(&bufferMutex);
	QByteArray datagrame(rcvBuffer);
	rcvBuffer.clear();
	return datagrame;
}

QByteArray Qsftp::read(qint64 maxSize)
{
	QMutexLocker locker(&bufferMutex);
	QByteArray datagrame = rcvBuffer.mid(0, maxSize);
	rcvBuffer.remove(0, datagrame.size());
	return datagrame;
}

bool Qsftp::initSsh2()
{
	return false;
}

bool Qsftp::sftp_init(const QString &filePath, int openType, LIBSSH2_SFTP_ATTRIBUTES * attrs)
{
	char *errbuff = NULL;
	int errlen = NULL;
	errStr.clear();
	int waitcount = 0;
	int rc = 0;
	//init sftpsession
	waitcount = SSH_CONNECT_WAITCOUNT;
	do {
		session->sftp_session = libssh2_sftp_init(session->session);

		if (!session->sftp_session) {
			if (libssh2_session_last_errno(session->session) ==
				LIBSSH2_ERROR_EAGAIN) {
				/* now we wait */
				rc = waitsocket(session->sock, session->session,0);
				waitcount--;
				if (rc < 0) {
					errStr = "Unable to init SFTP session";
					currentState = false;
					return false;
				}
			}
			else {
				errStr = "Unable to init SFTP session";
				currentState = false;
				return false;
			}

		}
	} while (!session->sftp_session && waitcount);
	if (!session->sftp_session && waitcount == 0) {
		errStr = "init SFTP session time out";
		currentState = false;
		return false;
	}

	if (attrs) {
		memset(attrs, 0, sizeof(LIBSSH2_SFTP_ATTRIBUTES));
		waitcount = SSH_CONNECT_WAITCOUNT;
		while ((rc = libssh2_sftp_lstat(session->sftp_session, filePath.toUtf8().data(), attrs)) == LIBSSH2_ERROR_EAGAIN && waitcount) {
			waitsocket(session->sock, session->session,0);
			waitcount--;
		}
		if (rc < 0) {
			errStr = "Unable to init SFTP stat";
			libssh2_sftp_shutdown(session->sftp_session);
			session->sftp_session = NULL;
			currentState = false;
			return false;
		}
	}

	/* Request a file via SFTP */
	waitcount = SSH_CONNECT_WAITCOUNT;
	do {
		if(openType == 0)
			session->sftp_handle = libssh2_sftp_open(session->sftp_session, filePath.toUtf8().data(),
				LIBSSH2_FXF_READ, 0);
		else
			session->sftp_handle =
			libssh2_sftp_open(session->sftp_session, filePath.toUtf8().data(),
				LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT,
				LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
				LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);

		if (!session->sftp_handle) {
			if (libssh2_session_last_errno(session->session) != LIBSSH2_ERROR_EAGAIN) {
				libssh2_session_last_error(session->session, &errbuff, &errlen, 0);
				errStr = QString::fromUtf8(errbuff, errlen);
				libssh2_sftp_shutdown(session->sftp_session);
				session->sftp_session = NULL;
				currentState = false;
				return false;
			}
			else {
				waitsocket(session->sock, session->session,0); /* now we wait */
				waitcount--;
			}
		}
	} while (!session->sftp_handle && waitcount);
	if (!session->sftp_handle && waitcount == 0) {
		errStr = "time out to open file with SFTP";
		libssh2_sftp_shutdown(session->sftp_session);
		session->sftp_session = NULL;
		currentState = false;
		return false;
	}
	return true;
}

//int Qsftp::waitsocket(int socket_fd, LIBSSH2_SESSION * session, qint64 sec)
//{
//	struct timeval timeout;
//	int rc;
//	fd_set fd;
//	fd_set *writefd = NULL;
//	fd_set *readfd = NULL;
//	int dir;
//
//	if (sec) {
//		timeout.tv_sec = sec;
//		timeout.tv_usec = 0;
//	}
//	else {
//		timeout.tv_sec = SSH_CONNECT_TIMEOUT_SEC;
//		timeout.tv_usec = SSH_CONNECT_TIMEOUT_USEC;
//	}
//
//	FD_ZERO(&fd);
//
//	FD_SET(socket_fd, &fd);
//
//	/* now make sure we wait in the correct direction */
//	dir = libssh2_session_block_directions(session);
//
//	if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
//		readfd = &fd;
//
//	if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
//		writefd = &fd;
//
//	rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);
//
//	return rc;
//}

void Qsftp::slot_connect()
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

void Qsftp::slot_disconnect()
{
	free_channel();

	session->disconnect();
}

void Qsftp::slot_sftpGet(QString filePath)
{
	int rc = 0;
	int waitcount = 0;
	bufferMutex.lock();
	rcvBuffer.clear();
	bufferMutex.unlock();
	if (session->errStr.size()) {
		emit ssh_error(errStr);
		currentState = false;
		return;
	}
	//init sftpsession
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	if (!sftp_init(filePath, 0, &attrs)) {
		emit ssh_error(errStr);
		return;
	}
	//now read from sftp
	qint64 rcvSize = 0;
	currentState = true;
	char mem[1024*512];
	do
	{
		do
		{
			rc = libssh2_sftp_read(session->sftp_handle, mem, sizeof(mem));
			if (rc > 0) {
				QMutexLocker locker(&bufferMutex);
				//qDebug() << "read from sftp:" << rc;
				rcvBuffer.append(mem, rc);
				locker.unlock();
				rcvSize += rc;
				emit ssh_downloadProgress(rcvSize, attrs.filesize);
				emit ssh_readyRead();
				//qDebug() << "rcv :" << rcvBuffer.size();
			}

		} while (rc > 0 && !session->quitFlag);

		if (rc != LIBSSH2_ERROR_EAGAIN) {
			/* error or end of file */
			break;
		}
		if (waitsocket(session->sock, session->session, 5) <= 0) {
			errStr = "read time out!";
			currentState = false;
			break;
		}

	} while (!session->quitFlag);

	if (!currentState)
		emit ssh_error(errStr);

	while ((rc = libssh2_sftp_close(session->sftp_handle)) == LIBSSH2_ERROR_EAGAIN);
	while ((rc = libssh2_sftp_shutdown(session->sftp_session)) == LIBSSH2_ERROR_EAGAIN);
	session->sftp_handle = NULL;
	session->sftp_session = NULL;
	emit ssh_finished();
}

void Qsftp::slot_sftpPut(QString filePath, const QString remotePath)
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

	//open file
	FILE *tempstorage = fopen(filePath.toUtf8().data(), "rb");
	if (!tempstorage) {
		currentState = false;
		errStr = QString("open file fail:%1").arg(strerror(errno));
		emit ssh_error(errStr);
		return;
	}

	if (!sftp_init(remotePath,1)) {
		emit ssh_error(errStr);
		return;
	}

	currentState = true;

	qint64 currentByte = 0;
	fseek(tempstorage, 0L, SEEK_END);
	qint64 totalByte = ftell(tempstorage);
	fseek(tempstorage, 0L, SEEK_SET);
	size_t nread;
	char mem[1024*512] = { 0 };
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
			/* write data in a loop until we block */
			waitcount = SSH_CONNECT_WAITCOUNT;
			while ((rc = libssh2_sftp_write(session->sftp_handle, ptr, nread)) ==
				LIBSSH2_ERROR_EAGAIN && waitcount) {
				if(waitsocket(session->sock, session->session, 1) > 0)
					continue;
				waitcount--;
			}
			if (rc < 0 && !waitcount) {
				currentState = false;
				errStr = QString("SFTP upload timed out: %1\n").arg(rc);
				break;
			}
			ptr += rc;
			nread -= rc;
		} while (nread && !session->quitFlag);
		emit ssh_uploadProgress(currentByte, totalByte);

	} while (currentState && !session->quitFlag);
	fclose(tempstorage);

	if (!currentState)
		emit ssh_error(errStr);

	while ((rc = libssh2_sftp_close(session->sftp_handle)) == LIBSSH2_ERROR_EAGAIN);
	while ((rc = libssh2_sftp_shutdown(session->sftp_session)) == LIBSSH2_ERROR_EAGAIN);
	session->sftp_handle = NULL;
	session->sftp_session = NULL;
	emit ssh_finished();
}

void Qsftp::slot_list(QString path)
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
			qDebug()<<textstream.readLine();
			//analyze the file info
			while (!textstream.atEnd() && !session->quitFlag)
			{
				LOCAL_FILE_INFO fileinfo;
				QStringList strlist = textstream.readLine().simplified().split(" ");
				if(strlist.size() < 8)
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

void Qsftp::slot_changedir(QString path)
{
	/*QByteArray resMsg, errMsg;
	if (!path.startsWith("cd"))
		path.insert(0, "cd ");
	path.append(";pwd");*/
	/*if (!exec_cmd(path, resMsg, errMsg))
		return;*/
	slot_list(path);
}

bool Qsftp::exec_cmd(const QString & cmd, QByteArray & resMsg, QByteArray &errMsg)
{
	create_channel();
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
	free_channel();
	return true;
}

inline void Qsftp::create_channel()
{
	//create ssh channel
	/* Exec non-blocking on the remove host */
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

	//while (libssh2_channel_request_pty(session->channel, "vanilla") == LIBSSH2_ERROR_EAGAIN);
	//while (libssh2_channel_shell(session->channel) == LIBSSH2_ERROR_EAGAIN);
}

inline void Qsftp::free_channel()
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
