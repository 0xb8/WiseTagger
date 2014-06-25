/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "reverse_search.h"
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>

ReverseSearch::ReverseSearch(QObject *_parent) : network_access_manager(this)
{
	Q_UNUSED(_parent);
	upload_progress.setLabelText(tr("Uploading file..."));
	upload_progress.setAutoClose(true);
	upload_progress.setCancelButton(nullptr);
}

ReverseSearch::~ReverseSearch()
{
	for(int i = 0; i < response_files.size(); ++i) {
		QFile(response_files[i]).remove();
	}
}

void ReverseSearch::search(const QString &file)
{
	current_file = file;
	upload_file();
}

void ReverseSearch::upload_file()
{
	QFileInfo file_info(current_file);

	if(file_info.size() > iqdb_max_file_size) {
		QMessageBox::warning(nullptr, tr("File is too large"),
			tr("<p>File is too large.</p><p>Maximum file size: <b>8Mb</b></p>"));
		return;
	}
	imagefile.setFileName(current_file);
	if (!imagefile.open(QIODevice::ReadOnly)){
		QMessageBox::critical(nullptr, tr("Cannot open file"),
			tr("Cannot open file %1 for uploading.").arg(current_file));
		return;
	}

	QUrl post_url(iqdb_post_url);
	QNetworkRequest post_request(post_url);
	QHttpMultiPart *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	static IqdbHttpParts iqdb_parts;

	iqdb_parts.file.setHeader(QNetworkRequest::ContentDispositionHeader,
		QString("form-data; name=\"file\"; filename=\"%1\"").arg(file_info.completeBaseName()).toLatin1());
	iqdb_parts.file.setHeader(QNetworkRequest::ContentTypeHeader,
		QMimeDatabase().mimeTypeForFile(file_info).name().toLatin1());
	iqdb_parts.file.setBodyDevice(&imagefile);

	multipart->append(iqdb_parts.maxsize);
	for(int i = 0; i < 12; ++i) {
		if(i > 6 && i < 10) continue;
		iqdb_parts.service.setBody(QString::number(i+1).toLatin1());
		multipart->append(iqdb_parts.service);
	}
	multipart->append(iqdb_parts.file);
	multipart->append(iqdb_parts.url);


	connect(&network_access_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(open_reply(QNetworkReply*)));
	QNetworkReply* r = network_access_manager.post(post_request, multipart);
	multipart->setParent(r);
	qDebug() << multipart->boundary();

	connect(r, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(showProgress(qint64,qint64)));
	upload_progress.show();
}


void ReverseSearch::open_reply(QNetworkReply* reply)
{
	if(reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) {
		QByteArray response = reply->readAll();
		QFileInfo curr_file(current_file);
		imagefile.close();
		QString response_filename = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		response_filename.append("/iqdb_");
		response_filename.append(curr_file.completeBaseName());
		response_filename.append(".html");
		response_files.push_back(response_filename);

		QFile rf(response_filename);
		rf.open(QIODevice::WriteOnly);
		rf.write(response);
		QDesktopServices::openUrl(QUrl::fromLocalFile(response_filename));
	}
	reply->deleteLater();
}

void ReverseSearch::showProgress(qint64 bytesSent, qint64 bytesTotal)
{
	upload_progress.setMaximum(bytesTotal);
	upload_progress.setValue(bytesSent);
}

IqdbHttpParts::IqdbHttpParts()
{
	maxsize.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"MAX_FILE_SIZE\"");
	maxsize.setBody(QString::number(ReverseSearch::iqdb_max_file_size).toLatin1());
	service.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"service[]\"");
	url.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"url\"");
	url.setBody("http://");
}
