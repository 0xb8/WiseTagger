/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef REVERSESEARCH_H
#define REVERSESEARCH_H

#include <QNetworkAccessManager>
#include <QProgressDialog>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QString>
#include <QVector>
#include <QFile>

class QNetworkReply;
class QObject;

struct IqdbHttpParts {
	IqdbHttpParts();
	QHttpPart maxsize;
	QHttpPart service;
	QHttpPart file;
	QHttpPart url;
};

class ReverseSearch : public QObject
{
	Q_OBJECT
public:
	ReverseSearch();
	~ReverseSearch();
	void search(const QString &file);
	void setProxy(const QString& protocol,
		const QString& host,
		std::uint16_t port,
		const QString& user = QString(),
		const QString& pass = QString());

	static constexpr char const* iqdb_post_url = "http://iqdb.org/";
	static const int max_file_size = 8388608; // 8 Mb

private slots:
	void open_reply(QNetworkReply* reply);
	void showProgress(qint64 bytesSent, qint64 bytesTotal);

private:
	void upload_file();

	QString current_file;
	QFile imagefile;
	QVector<QString> response_files;
	QNetworkAccessManager network_access_manager;
	QProgressDialog upload_progress;
};

#endif // REVERSESEARCH_H
