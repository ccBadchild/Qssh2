#include "QsshSession.h"
#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#else
#endif // 
#include <errno.h>
#include <QCoreApplication>
#include "libssh2_config.h"
#include "libssh2.h"
#include "libssh2_sftp.h"

#pragma execution_character_set("utf-8")

QsshSession::QsshSession(QObject *parent)
	: QObject(parent)
{
	static bool initOnce = false;
	if (!initOnce) {
		initOnce = true;
		qRegisterMetaType<LOCAL_FILE_INFO>("LOCAL_FILE_INFO");
		qRegisterMetaType<LOCAL_DIR_LIST>("LOCAL_DIR_LIST");
	}
}

QsshSession::~QsshSession()
{
}

bool QsshSession::connectToHost()
{
	struct sockaddr_in sin;
	int rc = libssh2_init(0);
	sock = socket(AF_INET, SOCK_STREAM, 0);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(hostPort);
	sin.sin_addr.s_addr = inet_addr(hostName.toUtf8().data());
	if (::connect(sock, (struct sockaddr*)(&sin),sizeof(struct sockaddr_in)) != 0) {
		connectState = false;
		errStr = QString("connect fail:%1").arg(strerror(errno));
		return connectState;
	}
	session = libssh2_session_init();
	if (!session) {
		connectState = false;
		errStr = "fail to init session";
		return connectState;
	}
	/* ... start it up. This will trade welcome banners, exchange keys,
	* and setup crypto, compression, and MAC layers
	*/

	rc = libssh2_session_handshake(session, sock);
	if (rc) {
		connectState = false;
		errStr = QString("Failure establishing SSH session:%1").arg(rc);
		return connectState;
	}

	libssh2_session_set_blocking(session, 0);

	if (password.size()) {
		while ((rc = libssh2_userauth_password(session, userName.toUtf8().data(), password.toUtf8().data()))
			== LIBSSH2_ERROR_EAGAIN)
			qApp->processEvents(QEventLoop::AllEvents, SSH_WAIT_TIMEOUT);
	}
	else {
		/* Or by public key */
		while ((rc =
			libssh2_userauth_publickey_fromfile(session, userName.toUtf8().data(),
				"/home/username/"
				".ssh/id_rsa.pub",
				"/home/username/"
				".ssh/id_rsa",
				password.toUtf8().data())) ==
			LIBSSH2_ERROR_EAGAIN)
			qApp->processEvents(QEventLoop::AllEvents, SSH_WAIT_TIMEOUT);
	}
	if (rc) {
		connectState = false;
		errStr = "Authentication by password failed.";
		return connectState;
	}
	connectState = true;
	return connectState;
}

bool QsshSession::disconnect()
{
	int rc = 0;
	if (session) {
		rc = libssh2_session_disconnect(session, "Normal Shutdown");
		rc = libssh2_session_free(session);
		session = NULL;
	}
#ifdef WIN32
	if(sock)
		rc = closesocket(sock);
	sock = 0;
#else
	close(sock);
#endif
	libssh2_exit();
	return true;
}

bool QsshSession::isConnect()
{
	return connectState;
}

bool QsshSession::hasLastErr()
{
	return !errStr.isEmpty();
}

const QString & QsshSession::getErrStr()
{
	return errStr;
}

int QsshSession::waitsocket(int socket_fd, LIBSSH2_SESSION * session, qint64 sec)
{
	struct timeval timeout;
	int rc;
	fd_set fd;
	fd_set *writefd = NULL;
	fd_set *readfd = NULL;
	int dir;

	if (sec) {
		timeout.tv_sec = sec;
		timeout.tv_usec = 0;
	}
	else {
		timeout.tv_sec = SSH_CONNECT_TIMEOUT_SEC;
		timeout.tv_usec = SSH_CONNECT_TIMEOUT_USEC;
	}

	FD_ZERO(&fd);

	FD_SET(socket_fd, &fd);

	/* now make sure we wait in the correct direction */
	dir = libssh2_session_block_directions(session);

	if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
		readfd = &fd;

	if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
		writefd = &fd;

	rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

	return rc;
}
