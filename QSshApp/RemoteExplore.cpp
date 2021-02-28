#include "RemoteExplore.h"
#include <QApplication>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QMessageBox>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QProgressDialog>
#include <QKeyEvent>
#include <QClipboard.h>
#include <QUrl>
#include <QDebug>
#include "Qsftp.h"
#include "Qscp.h"

RemoteExplore::RemoteExplore(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	//ui.tableWidget->setItemDelegate(new NoFocusDelegate());
	ui.lineEdit->setReadOnly(true);
	ui.tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget->verticalHeader()->setDefaultSectionSize(30);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	connect(ui.tableWidget, &QTableWidget::itemDoubleClicked, this, &RemoteExplore::table_doubleclicked);
	connect(ui.pushButton, &QPushButton::clicked, this, &RemoteExplore::slot_login);
	connect(ui.tableWidget, &QTableWidget::customContextMenuRequested, this, &RemoteExplore::slot_showmenu);
}

RemoteExplore::~RemoteExplore()
{
	if (sftpp)
	{
		delete sftpp;
		sftpp = NULL;
	}
}

void RemoteExplore::table_doubleclicked(QTableWidgetItem *item)
{
	QTableWidgetItem *p_item = ui.tableWidget->item(item->row(), 0);
	if (p_item && (p_item->data(Qt::UserRole).toString() == "d")) {
		QString path = p_item->text();
		QString currentpath = ui.lineEdit->text();
		qDebug() << path;
		if (currentpath == "/" && path == "..") {
			return;
		}
		else {
			if (currentpath == "/")
				path.insert(0, "/");
			else {
				path.insert(0, currentpath + "/");
			}
			if (ui.box_type->currentIndex() == 0)
				sftpp->changedir(path);
			else
				scpp->changedir(path);
		}
	}
}

void RemoteExplore::insertrow(const LOCAL_DIR_LIST & dirlist)
{
	ui.lineEdit->setText(dirlist.loacalPath);
	ui.tableWidget->clearContents();
	ui.tableWidget->setRowCount(0);
	foreach(LOCAL_FILE_INFO finf, dirlist.files) {
		int newrow = ui.tableWidget->rowCount();
		ui.tableWidget->insertRow(newrow);
		QTableWidgetItem *item = NULL;
		//name
		item = new QTableWidgetItem(finf.names);
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		if (finf.filetypes == 1) {
			item->setIcon(getFileIcon(QFileInfo(finf.names).completeSuffix()));
			item->setData(Qt::UserRole, "f");
		}
		else {
			item->setIcon(QFileIconProvider().icon(QFileIconProvider::Folder));
			item->setData(Qt::UserRole, "d");
		}
		ui.tableWidget->setItem(newrow, 0, item);

		//size
		item = new QTableWidgetItem;
		if (finf.filetypes == 1) {
			item->setText(QString::number(finf.filesize));
		}
		else {
			item->setText("-");
		}
		item->setTextAlignment(Qt::AlignCenter);
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui.tableWidget->setItem(newrow, 1, item);

		//modified
		item = new QTableWidgetItem(finf.date);
		item->setTextAlignment(Qt::AlignCenter);
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui.tableWidget->setItem(newrow, 2, item);

		//permissions
		item = new QTableWidgetItem(finf.permissions);
		item->setTextAlignment(Qt::AlignCenter);
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui.tableWidget->setItem(newrow, 3, item);

		//owner
		item = new QTableWidgetItem(finf.owner);
		item->setTextAlignment(Qt::AlignCenter);
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui.tableWidget->setItem(newrow, 4, item);
	}
}


QIcon RemoteExplore::getFileIcon(const QString &extension)
{
	QFileIconProvider provider;
	QIcon icon;
	QString strTemplateName = QDir::tempPath() + QDir::separator() +
		QCoreApplication::applicationName() + "_XXXXXX." + extension;
	QTemporaryFile tmpFile(strTemplateName);
	//tmpFile.setAutoRemove(false);

	if (tmpFile.open())
	{
		tmpFile.close();
		icon = provider.icon(QFileInfo(tmpFile.fileName()));
		// tmpFile.remove();
	}
	else
	{
		//qCritical() << QString("failed to write temporary file %1").arg(tmpFile.fileName());
	}

	return icon;
}

void RemoteExplore::slot_login()
{
	if (ui.pushButton->text() == "connect") {
		QString hostname = ui.edit_hostname->text();
		QString username = ui.edit_username->text();
		QString psd = ui.edit_psd->text();

		if (hostname.isEmpty()) {
			QMessageBox::information(this, "Info", "please enter your host name!");
			return;
		}
		if (username.isEmpty() || psd.isEmpty()) {
			QMessageBox::information(this, "Info", "please enter your user name or password!");
			return;
		}

		if (ui.box_type->currentIndex() == 0)
		{//current protocol is sftp
			if (!sftpp) {
				sftpp = new Qsftp(hostname);
				connect(sftpp, &Qsftp::ssh_connected, this, &RemoteExplore::slot_connected);
				connect(sftpp, &Qsftp::ssh_disconnected, this, &RemoteExplore::slot_disconnected);
				connect(sftpp, &Qsftp::ssh_downloadProgress, this, &RemoteExplore::slot_downloadProgress);
				connect(sftpp, &Qsftp::ssh_uploadProgress, this, &RemoteExplore::slot_uploadProgress);
				connect(sftpp, &Qsftp::ssh_error, this, &RemoteExplore::slot_error);
				connect(sftpp, &Qsftp::ssh_finished, this, &RemoteExplore::slot_finished);
				connect(sftpp, &Qsftp::ssh_dirChanged, this, &RemoteExplore::slot_dirChanged);
				sftpp->setUserName(username);
				sftpp->setPassword(psd);
			}
			sftpp->connectToHost();
		}
		else
		{//current protocol is scp
			if (!scpp) {
				scpp = new Qscp(hostname);
				connect(scpp, &Qscp::ssh_connected, this, &RemoteExplore::slot_connected);
				connect(scpp, &Qscp::ssh_disconnected, this, &RemoteExplore::slot_disconnected);
				connect(scpp, &Qscp::ssh_downloadProgress, this, &RemoteExplore::slot_downloadProgress);
				connect(scpp, &Qscp::ssh_uploadProgress, this, &RemoteExplore::slot_uploadProgress);
				connect(scpp, &Qscp::ssh_error, this, &RemoteExplore::slot_error);
				connect(scpp, &Qscp::ssh_finished, this, &RemoteExplore::slot_finished);
				connect(scpp, &Qscp::ssh_dirChanged, this, &RemoteExplore::slot_dirChanged);
				scpp->setUserName(username);
				scpp->setPassword(psd);
			}
			scpp->connectToHost();
		}
	}
	else {
		if (ui.box_type->currentIndex() == 0)
		{//current protocol is sftp
			sftpp->disconnect();
		}
		else
		{
			scpp->disconnect();
		}
	}
}

void RemoteExplore::slot_showmenu(const QPoint &pos)
{
	savefilename.clear();
	QTableWidgetItem *item = ui.tableWidget->itemAt(pos);
	if (!item)
		return;
	QString names = item->data(Qt::UserRole).toString();
	if (names != "f")
		return;
	QMenu menu;
	QAction *p_download = menu.addAction("download");
	if (menu.exec(QCursor::pos()) == p_download) {
		savefilename = QFileDialog::getSaveFileName(this, "Save as", QString("c:/").append(item->text()), "Files (*.*)");
		p_file = new QFile(savefilename);
		if (!p_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			QMessageBox::information(this, "info", p_file->errorString());
			savefilename.clear();
			p_file->deleteLater();
			return;
		}
		
		downProgress = new QProgressDialog(this);
		downProgress->setRange(0,100);
		p_file->setParent(downProgress);

		if (ui.box_type->currentIndex() == 0) {
			connect(sftpp, &Qsftp::ssh_finished, downProgress, &QProgressDialog::accept);
			connect(sftpp, &Qsftp::ssh_finished, p_file, &QFile::close);
			connect(sftpp, &Qsftp::ssh_downloadProgress, this, [=](qint64 received, qint64 total) {
				downProgress->setValue(((double)received / (double)total) * 100);
				//qDebug() << "rcv:" << received << " total:" << total;
			});
			connect(sftpp, &Qsftp::ssh_readyRead, this, [=]() {
				p_file->write(sftpp->readAll());
			});
			sftpp->sftpGet(ui.lineEdit->text() + "/" + item->text());
		}
		else {
			connect(scpp, &Qscp::ssh_readyRead, this, [=]() {
				p_file->write(scpp->readAll());
			});
			connect(scpp, &Qscp::ssh_finished, downProgress, &QProgressDialog::accept);
			connect(scpp, &Qscp::ssh_finished, p_file, &QFile::close);
			connect(scpp, &Qscp::ssh_downloadProgress, this, [=](qint64 received, qint64 total) {
				downProgress->setValue(((double)received / (double)total) * 100);
				//qDebug() << "rcv:" << received << " total:" << total;
			});
			connect(scpp, &Qscp::ssh_readyRead, this, [=]() {
				p_file->write(scpp->readAll());
			});
			scpp->scpGet(ui.lineEdit->text() + "/" + item->text());
		}

		downProgress->exec();
		downProgress->deleteLater();
	}
	
}

void RemoteExplore::slot_error(QString errstr)
{
	QMessageBox::warning(this, "Warning", errstr);
}

void RemoteExplore::slot_readyRead()
{
}

void RemoteExplore::slot_downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
}

void RemoteExplore::slot_uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
}

void RemoteExplore::slot_finished()
{
	savefilename.clear();
}

void RemoteExplore::slot_disconnected()
{
	ui.pushButton->setText("connect");
	ui.edit_hostname->setEnabled(true);
	ui.box_type->setEnabled(true);
	ui.edit_psd->setEnabled(true);
	ui.edit_username->setEnabled(true);
}

void RemoteExplore::slot_connected()
{
	ui.pushButton->setText("disconnect");
	ui.edit_hostname->setEnabled(false);
	ui.box_type->setEnabled(false);
	ui.edit_psd->setEnabled(false);
	ui.edit_username->setEnabled(false);
	if (ui.box_type->currentIndex() == 0) {
		sftpp->list();
	}
	else {
		scpp->list();
	}
}

void RemoteExplore::slot_dirChanged(LOCAL_DIR_LIST dirlist)
{
	LOCAL_FILE_INFO finf;
	finf.filetypes = 0;
	finf.names = "..";
	dirlist.files.insert(0, finf);
	insertrow(dirlist);
}

void RemoteExplore::keyPressEvent(QKeyEvent * event)
{
	if ((event->modifiers() == Qt::ControlModifier) && (event->key() == Qt::Key_V))
	{
		QClipboard *clipboard = QApplication::clipboard();   //获取系统剪贴板指针
		QString originalText = clipboard->text();
		QUrl url(originalText);
		if (url.isValid()) {
			QString filePath = url.path();
#ifdef WIN32
			filePath.startsWith("/") ? filePath.remove(0, 1) : filePath;
#endif
			QFileInfo info(filePath);
			if (info.isFile())
			{
				downProgress = new QProgressDialog(this);
				downProgress->setRange(0, 100);

				if (ui.box_type->currentIndex() == 0) {
					connect(sftpp, &Qsftp::ssh_finished, downProgress, &QProgressDialog::accept);
					connect(sftpp, &Qsftp::ssh_uploadProgress, this, [=](qint64 received, qint64 total) {
						downProgress->setValue(((double)received / (double)total) * 100);
						//qDebug() << "rcv:" << received << " total:" << total;
					});
					sftpp->sftpPut(filePath
						, ui.lineEdit->text() + info.fileName().insert(0, "/"));
				}
				else {
					connect(scpp, &Qscp::ssh_finished, downProgress, &QProgressDialog::accept);
					connect(scpp, &Qscp::ssh_uploadProgress, this, [=](qint64 received, qint64 total) {
						downProgress->setValue(((double)received / (double)total) * 100);
						//qDebug() << "rcv:" << received << " total:" << total;
					});
					scpp->scpPut(filePath
						, ui.lineEdit->text() + info.fileName().insert(0, "/"));
				}

				downProgress->exec();
				downProgress->deleteLater();
			}
		}
	}
	return QWidget::keyPressEvent(event);
}
