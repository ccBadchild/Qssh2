#include "Qssh.h"
#include <QThread>
#include "libssh2_config.h"
#include "libssh2.h"
#include "QsshSession.h"
#include <QDebug>

Qssh::Qssh(QObject *parent)
	: QObject(parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
}

Qssh::Qssh(const QString & host, int port, QObject * parent)
	:QObject(parent)
{
	waitsocket = &QsshSession::waitsocket;
	session = new QsshSession;
	session->hostName = host;
	session->hostPort = port;
}

Qssh::~Qssh()
{
	slot_disconnect();
	delete session;
	session = NULL;
}

void Qssh::setHostName(const QString & host)
{
	if (!session->isConnect())
		session->hostName = host;
}

void Qssh::setPort(int port)
{
	if (!session->isConnect())
		session->hostPort = port;
}

void Qssh::setUserName(const QString & username)
{
	if (!session->isConnect())
		session->userName = username;
}

void Qssh::setPassword(const QString & psd)
{
	if (!session->isConnect())
		session->password = psd;
}

const QString & Qssh::getHostName()
{
	return session->hostName;
}

bool Qssh::connectToHost()
{
	slot_connect();
	return session->isConnect();
}

void Qssh::disconnect()
{
	session->quitFlag = true;
	//session->disconnect();
	session->errStr.clear();
	session->connectState = false;
	currentState = false;
	errStr.clear();
	slot_disconnect();
	return;
}

void Qssh::exec(const QString & cmd)
{
	if (session->isConnect())
		slot_exec(cmd);
}

void Qssh::slot_connect()
{
	session->quitFlag = false;
	errStr.clear();

	if (!session->isConnect()) {
		if (!session->connectToHost()) {
			currentState = false;
			errStr = session->errStr;
			return;
		}
		else {
			currentState = true;
		}
	}
	if (session->channel) {
		return;
	}
	else {
		create_channel();
		if (!session->channel) {
			errStr = "channel was not create!";
			currentState = false;
		}
	}
}

void Qssh::slot_disconnect()
{
	free_channel();

	session->disconnect();
}

bool Qssh::slot_exec(QString cmd)
{
	errStr.clear();
	if (!session->channel) {
		create_channel();
		if (!session->channel) {
			errStr = "channel was not create!";
			return false;
		}
	}
	cmd += "\n";

	int waitcount = SSH_CONNECT_WAITCOUNT;
	int res = 0;
	QByteArray cmdArray = cmd.toUtf8();
	while ((res = libssh2_channel_write_ex(session->channel, 0, cmdArray.data(), cmdArray.size())) ==
		LIBSSH2_ERROR_EAGAIN &&
		waitcount && !session->quitFlag)
	{
		waitsocket(session->sock, session->session, 0);
		waitcount--;
	}

	if (res < 0) {
		if (waitcount == 0)
			errStr = "create ssh channel fail: request time out";
		else {
			char *errmsg = NULL;
			libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
		}
		return false;
	}
	return true;
}

qint64 Qssh::readData(QByteArray &rcvArray, const QByteArray &strend, int timeout)
{
	if (NULL == session->channel)
	{
		return 0;
	}

	LIBSSH2_POLLFD *fds = new LIBSSH2_POLLFD;
	fds->type = LIBSSH2_POLLFD_CHANNEL;
	fds->fd.channel = session->channel;
	//fds->events = LIBSSH2_POLLFD_POLLIN | LIBSSH2_POLLFD_POLLOUT;
	fds->events = LIBSSH2_POLLFD_POLLIN;

	while (timeout > 0)
	{
		int rc = (libssh2_poll(fds, 1, 10));
		if (rc < 1)
		{
			timeout -= 50;
			QThread::msleep(50);
			continue;
		}

		if (fds->revents & LIBSSH2_POLLFD_POLLIN)
		{
			size_t n = libssh2_channel_read(session->channel, buffer, sizeof(buffer));
			if (n == LIBSSH2_ERROR_EAGAIN)
			{
				//fprintf(stderr, "will read again\n");
			}
			else if (n <= 0)
			{
				return rcvArray.size();
			}
			else
			{
				rcvArray.append(buffer, n);
				if ("" == strend)
				{
					return rcvArray.size();
				}
				/*size_t pos = data.rfind(strend);
				if (pos != string::npos && data.substr(pos, strend.size()) == strend)
				{
					return data;
				}*/
			}
		}
		timeout -= 50;
		QThread::msleep(50);
	}
	return rcvArray.size();
}

inline void Qssh::create_channel()
{
	//create ssh channel
	/* Exec non-blocking on the remove host */
	int res = 0;
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
		return;
	}

	waitcount = SSH_CONNECT_WAITCOUNT;
	while ((res = libssh2_channel_request_pty(session->channel, "vanilla")) == LIBSSH2_ERROR_EAGAIN && waitcount)
	{
		waitsocket(session->sock, session->session,0);
		waitcount--;
	}

	if (res != 0) {

		currentState = false;
		if (waitcount == 0)
			errStr = "create ssh channel fail: request time out";
		else {
			char *errmsg = NULL;
			libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
		}
		free_channel();
		return;
	}

	waitcount = SSH_CONNECT_WAITCOUNT;
	while ((res = libssh2_channel_shell(session->channel)) == LIBSSH2_ERROR_EAGAIN && waitcount)
	{
		waitsocket(session->sock, session->session, 0);
		waitcount--;
	}

	if (res != 0) {
		currentState = false;
		if (waitcount == 0)
			errStr = "create ssh channel fail: request time out";
		else {
			char *errmsg = NULL;
			libssh2_session_last_error(session->session, &errmsg, NULL, 0);
			errStr = QString::fromUtf8(errmsg);
		}
		free_channel();
		return;
	}
}

inline void Qssh::free_channel()
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
