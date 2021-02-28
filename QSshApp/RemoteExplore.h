#pragma once

#include <QWidget>
#include "ui_RemoteExplore.h"
#include "ssh_global.h"


class QTableWidget;
class QTableWidgetItem;
class QProgressDialog;
class Qsftp;
class Qscp;
class QFile;

class RemoteExplore : public QWidget
{
	Q_OBJECT

public:
	RemoteExplore(QWidget *parent = Q_NULLPTR);
	~RemoteExplore();

private:
	Ui::RemoteExplore ui;
	QString savefilename;
	QProgressDialog *downProgress = NULL;
	QFile *p_file = NULL;
	Qsftp *sftpp = NULL;
	Qscp *scpp = NULL;

	void table_doubleclicked(QTableWidgetItem *item);

	void insertrow(const LOCAL_DIR_LIST &dirlist);

	QIcon getFileIcon(const QString & extension);

	void slot_login();
	void slot_showmenu(const QPoint&);

	void slot_error(QString errstr);
	void slot_readyRead();
	void slot_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void slot_uploadProgress(qint64 bytesSent, qint64 bytesTotal);
	void slot_finished();
	void slot_disconnected();
	void slot_connected();
	void slot_dirChanged(LOCAL_DIR_LIST);
protected:
	void keyPressEvent(QKeyEvent *event);
};
