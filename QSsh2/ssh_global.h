#pragma once
#include <QtCore/QtCore>
#include "libssh2.h"
#define SSH_WAIT_TIMEOUT									100
#define SSH_CONNECT_TIMEOUT_SEC								1
#define SSH_CONNECT_TIMEOUT_USEC							0
#define SSH_CONNECT_WAITCOUNT								5

#define SSH_CMD_PWD											"pwd"
#define SSH_CMD_LIST										"ls -l --time-style=long-iso "

typedef struct _LIBSSH2_SESSION								LIBSSH2_SESSION;
typedef struct _LIBSSH2_CHANNEL								LIBSSH2_CHANNEL;
typedef struct _LIBSSH2_LISTENER							LIBSSH2_LISTENER;
typedef struct _LIBSSH2_KNOWNHOSTS							LIBSSH2_KNOWNHOSTS;
typedef struct _LIBSSH2_AGENT								LIBSSH2_AGENT;

typedef struct _LIBSSH2_SFTP								LIBSSH2_SFTP;
typedef struct _LIBSSH2_SFTP_HANDLE							LIBSSH2_SFTP_HANDLE;
typedef struct _LIBSSH2_SFTP_ATTRIBUTES						LIBSSH2_SFTP_ATTRIBUTES;
typedef struct _LIBSSH2_SFTP_STATVFS						LIBSSH2_SFTP_STATVFS;


typedef struct _LOCAL_FILE_INFO {
	//0 dir 1 file
	int														filetypes = 0;
	QString													names;
	QString													owner;
	QString													permissions;
	QString													date;
	qint64													filesize = 0;
}LOCAL_FILE_INFO;


typedef struct _LOCAL_DIR_LIST {
	QString													loacalPath;
	QVector<LOCAL_FILE_INFO>								files;
}LOCAL_DIR_LIST;