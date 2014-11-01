/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "reverse_search.h"
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QMimeDatabase>
#include <QMessageBox>
#include <stdexcept>
#include <QFileInfo>
#include <QUrl>

#include "util/config.h"

ReverseSearch::ReverseSearch()
{
	upload_progress.setLabelText(tr("Uploading file..."));
	upload_progress.setAutoClose(true);
	upload_progress.setCancelButton(nullptr);
}

ReverseSearch::~ReverseSearch()
{
	for(int i = 0; i < response_files.size(); ++i) {
		QFile::remove(response_files[i]);
	}
}

void ReverseSearch::search(const QString &file)
{
	current_file = file;
	upload_file();
}

void ReverseSearch::setProxy(const QString &protocol, const QString &host,
	std::uint16_t port, const QString &user, const QString &pass)
{
	QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;
	if(protocol == "socks5")
		type = QNetworkProxy::Socks5Proxy;
	else if(protocol == "http")
		type = QNetworkProxy::HttpProxy;
	/* This is checked in calling code and should never happen. */
	else throw std::invalid_argument(tr("Invalid proxy protocol: %1").arg(protocol).toStdString());

	QNetworkProxy proxy(type, host, port, user, pass);
	network_access_manager.setProxy(proxy);
}

void ReverseSearch::upload_file()
{
	QFileInfo file_info(current_file);
	if(file_info.size() > max_file_size) {
		QMessageBox::warning(nullptr, tr("File is too large"),
			tr("<p>File is too large.</p><p>Maximum file size: <b>8Mb</b></p>"));
		return;
	}

	imagefile.setFileName(current_file);
	if (!imagefile.open(QIODevice::ReadOnly)){
		QMessageBox::critical(nullptr, tr("Cannot open file"),
			tr("<p>Cannot open file <b>%1</b> for uploading.</p>").arg(current_file));
		return;
	}

	QUrl post_url(iqdb_post_url);
	QNetworkRequest post_request(post_url);
	QHttpMultiPart *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	static IqdbHttpParts iqdb_parts;
	/* Add filename, mime type and file data to file part */
	iqdb_parts.file.setHeader(QNetworkRequest::ContentDispositionHeader,
		QString("form-data; name=\"file\"; filename=\"%1\"")
			.arg(file_info.completeBaseName()).toLatin1());

	iqdb_parts.file.setHeader(QNetworkRequest::ContentTypeHeader,
		QMimeDatabase().mimeTypeForFile(file_info).name().toLatin1());

	iqdb_parts.file.setBodyDevice(&imagefile);

	/* Check IQDB search services */
	for(int i = 0; i < 12; ++i) {
		if(i > 6 && i < 10) continue;
		iqdb_parts.service.setBody(QString::number(i+1).toLatin1());
		multipart->append(iqdb_parts.service);
	}
	multipart->append(iqdb_parts.maxsize);
	multipart->append(iqdb_parts.file);
	multipart->append(iqdb_parts.url);

	connect(&network_access_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(open_reply(QNetworkReply*)));
	QNetworkReply* r = network_access_manager.post(post_request, multipart);
	multipart->setParent(r);

	connect(r, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(showProgress(qint64,qint64)));
	upload_progress.exec();
}


void ReverseSearch::open_reply(QNetworkReply* reply)
{
	if(reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) {
		QByteArray response = reply->readAll();
		QFileInfo curr_file(current_file);
		/* At this point request been sent, so we don't need this anymore */
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
	maxsize.setHeader(QNetworkRequest::ContentDispositionHeader,
		"form-data; name=\"MAX_FILE_SIZE\"");

	service.setHeader(QNetworkRequest::ContentDispositionHeader,
		"form-data; name=\"service[]\"");

	url.setHeader(QNetworkRequest::ContentDispositionHeader,
		"form-data; name=\"url\"");

	maxsize.setBody(QString::number(ReverseSearch::max_file_size).toLatin1());
	url.setBody("http://");
}
